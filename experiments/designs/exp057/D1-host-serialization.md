# D1 - Host serialization in the decode critical path (per-node ids readback + ggml_backend_synchronize)

Design document for EXP-057 (offline, read-only). Author: codebase inspection agent.
Date: 2026-09-15.
Accepted runtime source: work/llama.cpp-exp046-default
Reference build: build/exp046-default-runtime/bin/llama-server

This document is a STATIC source design. No build, no model, no server, no profiler run was
performed. No file in the repository or in work/ or build/ was modified. Only this file was
written, under /tmp/exp057-designs/.

All line numbers below refer to the accepted tree
/home/end0/AI/FrankenMoE-CUDA/work/llama.cpp-exp046-default unless stated otherwise.
The patches at the end are text only and were NOT applied.

--------------------------------------------------------------------------------
## 0. Summary of the finding (read this first)

The site named in the assignment, the per-node
`ggml_backend_tensor_get_async(ids_backend, ids_tensor, ...)` followed by
`ggml_backend_synchronize(ids_backend)` at ggml-backend.cpp:1673-1674, is inside the
"weights branch" of the scheduler (ggml-backend.cpp:1643-1741). That branch only runs when
a host (CPU) expert weight tensor is a split input of a GPU split, which happens only when
the MUL_MAT_ID node itself is offloaded to CUDA. CUDA accepts MUL_MAT_ID only when the batch
is >= 32 (`GGML_OP_OFFLOAD_MIN_BATCH`, default 32). Decode verify is a 3-token batch, so in
decode the expert MUL_MAT_ID stays on the CPU, the host weights are used in place, no expert
range is staged, and ggml-backend.cpp:1673-1674 is NOT executed.

Therefore: for the current decode runtime, the assignment's specific call site is a PREFILL
site, not a decode site. It cannot be a component of the decode budget.

Decode does have an equivalent and very frequent per-node full-stream synchronization, but it
lives in the generic split-input copy path of the same function, ggml-backend.cpp:1745-1752,
which every GPU<->CPU input crossing goes through. The rest of this document localizes that
path, quantifies it from the existing measurements, evaluates the three candidate directions
from the assignment against the real code, and proposes a safe change for the decode path.

The short answer to the goal question: the per-node synchronize is real and is a host-drain in
decode, but in the current single-copy scheduler it is mostly a required producer wait plus a
redundant implementation detour. Its removable share is bounded by roughly the API time of
the synchronize calls (about 4.9% of a 129.5 ms twin window in EXP-048, of which only a part
is removable). It cannot account for the 24-28% host-serialization estimate, which is
dominated by the 4559 individual kernel launches and the CPU barriers, not by this sync.

--------------------------------------------------------------------------------
## 1. Exact call sites

### 1.1 The weights branch (prefill only) - ggml-backend.cpp:1643-1741

Branch condition (ggml-backend.cpp:1643-1650):

    // when offloading MoE weights, we can reduce the amount of data copied by copying only the experts that are used
    ggml_tensor * node = split->graph.nodes[0];
    if (split->graph.n_nodes > 0 &&
        ggml_backend_buffer_get_usage(input->buffer) == GGML_BACKEND_BUFFER_USAGE_WEIGHTS &&
        ggml_backend_buffer_is_host(input->buffer) && (
        (node->src[0] == input_cpy && node->op == GGML_OP_MUL_MAT_ID)
        //|| (node->src[1] == input_cpy && node->op == GGML_OP_ADD_ID) /* GGML_OP_ADD_ID weights are small and not worth splitting */
        )) {

This code is inside `for (int input_id = 0; input_id < split->n_inputs; input_id++)`
(ggml-backend.cpp:1622). It is only reachable when the host expert weight tensor is one of
`split->inputs`, i.e. when the split backend cannot use the host buffer directly.

The ids readback named in the assignment (ggml-backend.cpp:1655-1674):

    ggml_backend_synchronize(input_backend);

    // get the ids
    ggml_tensor * ids_tensor = node->src[2];
    ggml_backend_t ids_backend = split_backend;

    // if the ids tensor is also an input of the split, it may not have been copied yet to the split backend
    // in that case, we use the original ids tensor
    for (int i = input_id + 1; i < split->n_inputs; i++) {
        if (ids_tensor == tensor_copy(split->inputs[i], split_backend_id, sched->cur_copy)) {
            ids_tensor = split->inputs[i];
            ids_backend = ggml_backend_sched_get_tensor_backend(sched, split->inputs[i]);
            break;
        }
    }

    if (ids_tensor != prev_ids_tensor) {
        ids.resize(ggml_nbytes(ids_tensor) / sizeof(int32_t));
        ggml_backend_tensor_get_async(ids_backend, ids_tensor, ids.data(), 0, ggml_nbytes(ids_tensor));
        ggml_backend_synchronize(ids_backend);

        // find the used experts
        used_ids.clear();
        used_ids.resize(ggml_bitset_size(n_expert));
        ...
        prev_ids_tensor = ids_tensor;
    }

Then the selected ranges are staged by `copy_experts` (ggml-backend.cpp:1699-1741), which calls
`set_tensor_async_pinned_ring` (the EXP-038/EXP-045 ring) and otherwise
`ggml_backend_tensor_set_async`.

### 1.2 Why this branch is not on the decode path

Backend selection for an op that has a WEIGHTS source (ggml-backend.cpp:958-967):

    if (src->buffer != NULL && src->buffer->usage == GGML_BACKEND_BUFFER_USAGE_WEIGHTS) {
        int src_backend_id = ggml_backend_sched_backend_from_buffer(sched, src, tensor);
        // check if a backend with higher prio wants to offload the op
        if (sched->op_offload && src_backend_id == sched->n_backends - 1 && ggml_backend_buffer_is_host(src->buffer)) {
            for (int b = 0; b < src_backend_id; b++) {
                if (ggml_backend_supports_op(sched->backends[b], tensor) && ggml_backend_offload_op(sched->backends[b], tensor)) {
                    SET_CAUSE(tensor, "1.off");
                    return b;
                }
            }
        }

CUDA decides offloadable with (ggml-cuda.cu:5569-5588, 5758):

    case GGML_OP_MUL_MAT_ID:
    ...
        return op->ne[2];
    ...
    return get_op_batch_size(op) >= dev_ctx->op_offload_min_batch_size;
    ...
    const int min_batch_size = getenv("GGML_OP_OFFLOAD_MIN_BATCH") ? atoi(getenv("GGML_OP_OFFLOAD_MIN_BATCH")) : 32;

For MUL_MAT_ID, `get_op_batch_size` returns `op->ne[2]`, which is the number of tokens. Decode
verify is 3 tokens; the MTP draft pass is 1 token; both are < 32. The op stays on the CPU.
When the split backend can use the host buffer directly, no copy is created
(ggml-backend.cpp:1401 gates the copy on `!ggml_backend_sched_buffer_supported(...)`), so the
host weights never appear in `split->inputs` and the loop at 1622 never evaluates the weights
branch for them.

This is independently confirmed by the existing offline analyses:

- experiments/EXP-2026-09-09-048-decode-remaining-operations.md:24:
  "a batch < 32 keeps the MUL_MAT_ID experts on the CPU (GGML_OP_OFFLOAD_MIN_BATCH=32, ggml-cuda.cu:5641)".
- experiments/EXP-2026-09-07-043-prefill-bottleneck-profile.md:304:
  "48 layers x [GPU cluster router+dense 0.5-3.4 ms (34x87 and 11x116 cores) + 2 small D2H per layer
   (48x40 KiB + 48x6 KiB - readback of ids/activations for the CPU split) + CPU MUL_MAT_ID of experts]".
- experiments/EXP-2026-09-07-043-prefill-bottleneck-profile.md:321-324:
  "the 391-token prefill stages experts on the GPU (17886 gathers), while the 4-token batch computes
   experts on the CPU with per-layer synchronizations/readbacks".

Conclusion: ggml-backend.cpp:1673-1674 is a prefill site. The 24-28% decode estimate cannot be
attributed to it.

### 1.3 The real per-node synchronize in decode - ggml-backend.cpp:1745-1752

The generic split-input copy fallback (ggml-backend.cpp:1742-1754):

    } else {
        // try async copy, but if not possible, we can still use a sync copy without synchronizing the dst backend, since we handle the synchronization here with multiple copies and events
        // TODO: add public function to facilitate this, since applications do not have direct access to the backend interface
        if (!split_backend->iface.cpy_tensor_async || !split_backend->iface.cpy_tensor_async(input_backend, split_backend, input, input_cpy)) {
            ggml_backend_synchronize(input_backend);
            if (sched->events[split_backend_id][sched->cur_copy] != NULL) {
                ggml_backend_event_synchronize(sched->events[split_backend_id][sched->cur_copy]);
            } else {
                ggml_backend_synchronize(split_backend);
            }
            ggml_backend_tensor_copy(input, input_cpy);
        }
    }

For a GPU->CPU crossing in decode (the router ids and the MoE input activations that the CPU
split consumes):
- `split_backend` is the CPU backend, whose `cpy_tensor_async` is NULL
  (ggml-cpu.cpp:200-201), so the condition is true.
- `ggml_backend_synchronize(input_backend)` (line 1746) is a full
  `cudaStreamSynchronize(cuda_ctx->stream())` (ggml-cuda.cu:2771-2774).
- Because the CPU backend creates no events (`event_new = NULL`, ggml-cpu.cpp:500-502) and,
  more importantly, because the scheduler only allocates events when `n_copies > 1`
  (ggml-backend.cpp:1865-1869), the `sched->events[...]` test is false and line 1750 executes
  a no-op CPU synchronize.
- `ggml_backend_tensor_copy` (ggml-backend.cpp:479-500) takes the host-destination path
  (line 488-489) and calls `ggml_backend_tensor_get`, which for a CUDA buffer does
  (ggml-cuda.cu:815-821):

    CUDA_CHECK(cudaMemcpyAsync(data, (const char *) tensor->data + offset, size, cudaMemcpyDeviceToHost, cudaStreamPerThread));
    CUDA_CHECK(cudaStreamSynchronize(cudaStreamPerThread));

  i.e. a second, per-thread-stream synchronize.

For a CPU->GPU crossing (the CPU expert output consumed by the next GPU split):
- line 1746 is a no-op CPU synchronize;
- line 1747 tests the events of the GPU backend. With `n_copies == 1` those are NULL, so line
  1750 executes a full `cudaStreamSynchronize` on the GPU compute stream;
- `ggml_backend_tensor_copy` takes the host-source path (line 486-487) and calls
  `ggml_backend_tensor_set`, which for a CUDA buffer does (ggml-cuda.cu:807-813):

    CUDA_CHECK(cudaMemcpyAsync((char *) tensor->data + offset, data, size, cudaMemcpyHostToDevice, cudaStreamPerThread));
    CUDA_CHECK(cudaStreamSynchronize(cudaStreamPerThread));

  another per-thread-stream synchronize.

So every GPU<->CPU split-input crossing costs two host-blocking stream synchronizations in the
current single-copy configuration. The event fast paths in the same function
(ggml-backend.cpp:1614, 1629, 1637, 1747, 1798) are dead code for this runtime.

### 1.4 Why the event fast paths are dead: n_copies == 1

ggml-backend.cpp:1832:

    sched->n_copies = parallel ? GGML_SCHED_MAX_COPIES : 1;

ggml-backend.cpp:1865-1869:

    if (sched->n_copies > 1) {
        for (int c = 0; c < sched->n_copies; c++) {
            sched->events[b][c] = ggml_backend_event_new(backends[b]->device);
        }
    }

The scheduler is created with `parallel = cparams.pipeline_parallel`
(src/llama-context.cpp:655). `pipeline_parallel` requires `model.n_devices() > 1`
(src/llama-context.cpp:428-433). This server has one CUDA device, and
`cparams.pipeline_parallel = false` by default (src/llama-context.cpp:276). Therefore
`n_copies == 1`, no events are allocated (they stay calloc-zero), and each `sched->events[...]`
check is false.

Consequence: the "event/fence scoped to the copy" direction already has an implementation in
the file, but it is inactive for the accepted runtime. Activating it is the smallest lever.

--------------------------------------------------------------------------------
## 2. Dependency argument: what is required and what could legally overlap

Required dependencies (cannot be removed):

1. ids are an input of the expert-range decision. The host must know which experts the router
   selected before it can build `used_ids` and submit `copy_experts`. This is a true
   data dependency ids -> expert copy.
2. On a single stream, the ids D2H is ordered after the kernels that produce them
   (router matmul + argsort). The host cannot read ids before that producer finish.
3. The CPU split must have the MoE input activation on the host before `MUL_MAT_ID` runs on
   the CPU. That is a true data dependency activation -> CPU compute.
4. The GPU must not overwrite an input copy while the CPU is still reading it, and vice versa.

What could legally overlap, in the current structure:

- The two small readbacks of a CPU split (activation and ids) are produced by the same
  preceding GPU split. Their producer wait is shared, so one host wait per split is enough;
  the second full-stream synchronize is redundant.
- The D2H itself can run on the compute stream (`ggml_backend_tensor_get_async` does exactly
  that, ggml-cuda.cu:2681-2688) instead of the blocking per-thread-stream path. The host wait
  is then scoped to the copy, not to a fresh stream drain.
- For CPU->GPU copies, the destination safety wait can be expressed as a stream event wait
  (`cudaStreamWaitEvent`) instead of a host `cudaStreamSynchronize`, so the host does not
  block before enqueueing the H2D and the next split.

What cannot overlap in this runtime:

- The next MoE layer's router/argsort depends on the current layer's expert output, so the
  next layer's GPU cluster cannot be launched during the current CPU expert block. Reading
  the ids "one split earlier" is not possible: the producer is the immediately preceding
  split of the same pass, and there is no earlier enqueue point.
- While the CPU computes experts, the GPU has no independent work queued; the 84% GPU idle in
  the EXP-048 twin window is structural, not caused by this synchronize.

--------------------------------------------------------------------------------
## 3. Candidate changes

### Candidate A (recommended, decode relevant): activate the scheduler events for n_copies == 1

Change: allocate the per-backend scheduler events even when there is a single copy, so the
existing event paths (ggml-backend.cpp:1614, 1629, 1637, 1747, 1798) become active.

Correctness argument: the events are recorded after each split (line 1798) and waited before
the next use of the same backend's copies. With one copy the same event for a backend is
re-recorded once per pass; every wait is issued in program order before the next record on
that backend, and CUDA captures the event state at `cudaStreamWaitEvent` call time, so a wait
cannot observe a later record. The CPU backend creates no events, so for CPU destinations the
existing no-op fallbacks remain. The bytes moved and the graph are unchanged.

Failure modes: a misplaced record could let the host overwrite an input copy still in use by
the device; or a stream wait could be captured too early. Both are caught by the byte-equality
and repeat-cycle tests in section 4. This is the change most likely to be rejected upstream
("changes scheduler semantics"), so it must stay behind a flag for the experiment.

### Candidate B (recommended, decode relevant): route host<->device split-input copies through the async interface

Change: in the ggml-backend.cpp:1745 fallback, detect host<->device crossings and use
`ggml_backend_tensor_get_async` / `ggml_backend_tensor_set_async` on the source/destination
compute stream instead of the blocking `ggml_backend_tensor_copy`, keeping the current
fallback for every other case.

Correctness argument: the async interface is the same one the weights branch already uses
(ggml-backend.cpp:1673, 1711). `ggml_backend_cuda_get_tensor_async` and `set_tensor_async`
operate on `cuda_ctx->stream()` (ggml-cuda.cu:2460-2467, 2681-2688), the same stream the
split's compute is launched on, so ordering is preserved by stream order. The host source of
an H2D is kept alive by the `input_dep` node added at ggml-backend.cpp:1483-1486. For a D2H
the host waits with one backend synchronize after the copy.

Failure modes: a backend whose async interface is absent must keep the old path; a pageable
destination may still cause a blocking copy, so the win must be measured, not assumed; a
non-CUDA accelerator with a different stream model could lose ordering.

### Candidate C (rejected): cache the readback keyed on tensor identity across steps

Rejected for correctness. Within one `ggml_backend_sched_compute_splits` call the code already
caches with `prev_ids_tensor` (ggml-backend.cpp:1671-1688), which is safe because a single
pass visits each ids tensor once. Extending the cache across calls is unsafe: the ids tensor
object is reused and its data is rewritten every step, so pointer identity does not imply
identical contents. A stale cache would drop experts and silently corrupt routing. There is no
version counter on the tensor to key on.

### Candidate D (rejected for decode): issue the ids readback one split earlier

Rejected. The ids producer is the immediately preceding split of the same pass; the scheduler
launches splits strictly in order and the CPU split's `ggml_backend_graph_compute_async`
(ggml-backend.cpp:1759) is synchronous for the CPU backend, so the next GPU split cannot be
launched early either. "One node earlier" in enqueue order does not change stream order on a
single stream and would not make the synchronize already satisfied.

--------------------------------------------------------------------------------
## 4. How many synchronizes per decode step

Structure: 48 MoE layers (EXP-049: 49 GPU splits = 48 layers + logits; EXP-043/048: 48 layers).
Per decode verify pass (batch 3) the scheduler alternates GPU cluster and CPU expert splits.

Per MoE layer, GPU<->CPU crossings and their host syncs (single-copy config):

| Crossing | Direction | Quantity/layer | Host stream syncs |
| --- | --- | ---: | ---: |
| MoE input activation -> CPU split | GPU->CPU | 1 | 1 compute-stream (1746) + 1 per-thread (get_tensor) |
| router ids -> CPU split | GPU->CPU | 1 | reused producer wait (1746, usually already done) + 1 per-thread (get_tensor) |
| CPU expert output -> next GPU split | CPU->GPU | 1 | 1 compute-stream (1750, events NULL) + 1 per-thread (set_tensor) |
| expert weight upload | CPU->GPU | 0 in decode (batch < 32) | 0 |

So roughly 3-4 full stream synchronizes per MoE layer, about 48 more per layer-set, i.e.
about 150-200 per 48-layer pass from this path. EXP-048 measured the whole 4-token twin
window at 396 `cudaStreamSynchronize` calls, 6.405 ms host time, mean 16.17 us, over a
129.5 ms window (offline-results.txt:9). The remainder comes from logits/sampler readbacks
and other CUDA API in the window. Many of the calls are no-op waits (EXP-043: 391 draft-row
syncs at 0.72 us each, experiments/EXP-2026-09-07-043-prefill-bottleneck-profile.md:301).

Distinguish host nodes from GPU splits:

- Host (CPU) nodes: the expert `MUL_MAT_ID` per layer. They do not issue CUDA syncs themselves,
  but every host node is reached through a GPU->CPU input crossing (the two readbacks), and its
  output leaves through a CPU->GPU crossing. That is the source of the per-layer syncs.
- GPU splits: the dense cluster plus router plus argsort. Launching them is the 4559
  individual `cudaLaunchKernel` calls (13.5 ms) of EXP-048, a separate and larger sub-block.

--------------------------------------------------------------------------------
## 5. Expected effect, stated as a range with unknowns

Anchors from existing measurements (offline, not re-measured):

- EXP-048, W2 4-token twin window 129.5 ms: 396 `cudaStreamSynchronize`, 6.405 ms API sum =
  4.9% of the window (mean 16.17 us; experiments/EXP-2026-09-09-048-...md:56,
  results/archive/EXP-2026-09-09-048/offline-results.txt:9).
- The same window: GPU busy 20.301 ms (15.7%), remainder 73-81% is CPU compute and host logic.
- The expert block is about 56-60% of the step; the GPU swarms 16-19%; the host serialization
  upper estimate is 24-28% (EXP-048:74).

Model:

- Candidate A + B remove the redundant second wait per crossing and the per-thread-stream
  detours. Upper bound: all 396 synchronize API time is 4.9% of the window; the part issued by
  this path is perhaps half to two thirds, and not all of it is removable (a producer wait for
  a D2H is a real dependency).
- Realistic removable range: about 1% to 4% of the decode step, central estimate about 2%.
  This is below or at the 2% acceptance threshold and must be measured.
- This does NOT recover the 24-28% host-serialization estimate. That figure is dominated by
  the 4559 individual kernel launches (13.5 ms API, 10.4% of the window) and the CPU barriers
  (EXP-014/020: 39.96% of cycles in libgomp spin, localized in the per-node graph barrier).
  The CUDA-graph direction for target verify was already closed by EXP-049/050/051/052.

Explicit unknowns:

1. How many of the 396 syncs are no-op versus waiting on real GPU work. No call stacks in the
   saved trace (EXP-043:330-334 names this as an open gap).
2. Whether an async D2H into a pageable ggml-alloc buffer is truly asynchronous on this driver;
   if not, candidate B's D2H benefit is near zero without a pinned staging buffer.
3. The precise split structure at batch 3 (number of CPU splits per layer) is inferred from the
   4-token twin, not measured for the current MTP decode.
4. Whether enabling events for n_copies == 1 perturbs graph reuse or CUDA-graph capture in
   llama (the events are a scheduler-level object, but this must be checked).

Amdahl note: even a perfect removal of this path is capped by the fixed CPU expert compute
(56-60% of the step). The step cannot drop below that block without also addressing IQ2_S/IQ4_NL.

--------------------------------------------------------------------------------
## 6. Model-free test plan (execution phase, separate approval; no model, no server)

Reuse the EXP-038 fixture style (results/archive/EXP-2026-09-06-038/tests/test-exp038-ring.cpp):
a real `ggml_backend_sched` over a CUDA backend and a CPU backend, small graph, deterministic
ids.

1. Branch-location test. Build a graph with a host WEIGHTS expert tensor, a GPU-produced
   int32 selected_experts tensor, and a MUL_MAT_ID node.
   a. batch 3: assert the weights branch counter is 0 and the generic copy path counter is
      > 0. Proves ggml-backend.cpp:1673-1674 is not on the decode path.
   b. batch 64: assert the weights branch counter is > 0. Proves the site is prefill.
   c. Assert `node->ne[2] == n_tokens` for MUL_MAT_ID so the gate arithmetic is checked.
2. Sync-count test. Instrument `ggml_backend_synchronize` with a counter and record the count
   per pass, per backend, before and after candidate A/B. Assert the count drops and that no
   pass has a sync between a copy and its consumer in the wrong order.
3. Events-active test. With candidate A, assert `sched->events[b][0] != NULL` for the GPU
   backend and that the `ggml_backend_event_wait`/`event_synchronize` paths are taken;
   assert the CPU backend still uses its no-op fallback.
4. Byte-equality test. For sparse and consecutive ids, first and last expert, compare the CPU
   split input bytes and the final graph output byte-for-byte across off/on, including with
   candidate B's async get/set. Include the MMQ padding case for prefill only.
5. Repeat-cycle test. Run the same graph at least 8 times alternating shapes 3 and 1 to catch
   event re-record ordering bugs and stale input copies.
6. Fallback test. Null out the async interfaces (or use a backend without them) and assert the
   original blocking path is used and output is unchanged.
7. Lifetime test. Assert the host source of an H2D stays alive until the copy completes
   (the input_dep node), and that no copy remains in flight at scheduler free.

Only after these gates may a paired decode measurement be proposed, subject to AGENTS.md
hard rule 1 (explicit approval for each server run).

--------------------------------------------------------------------------------
## 7. Risks and what would falsify the hypothesis

Risks:

1. Candidate A changes scheduler synchronization semantics for all single-copy runtimes, not
   only this server. A mistake can cause use-after-overwrite races that are hard to see.
2. Candidate B changes a generic copy path; non-CUDA accelerators with different stream models
   could regress.
3. Pageable D2H may not overlap, making candidate B's gain negligible.
4. The measured 396 syncs include waits for genuinely required GPU work; removing the call
   does not remove the wait.
5. Amdahl cap: the CPU expert block dominates, so even a perfect fix is small.
6. Event allocation increases scheduler state slightly; this is not a memory concern but is a
   behavioral change in upstream code.

Falsification criteria:

- If instrumentation shows the entire ids + activation readback path costs less than about
  2% of the decode step in host time, then the claim "the per-node ids readback plus
  synchronize is a material removable serial component of decode" is FALSE.
- If an implemented candidate yields a decode change below the 2% acceptance threshold in
  paired runs (or within noise), the hypothesis is FALSIFIED for this site.
- If the weights branch is shown to run at batch 3 for this model (it is not, per the gate and
  EXP-043/048), the prefill/decode attribution above would be wrong and must be re-derived.
- If enabling events for n_copies == 1 fails the repeat-cycle byte-equality test, candidate A
  is REJECTED regardless of speed.

--------------------------------------------------------------------------------
## 8. Proposed patch, AS TEXT, NOT APPLIED

Two independent, small patches. P1 is the enabling change; P2 is the copy routing. Both are
presented for review only. They were not applied, not built, and not tested. Keep each behind
an experiment flag in the real implementation.

### P1 - activate scheduler events for the single-copy configuration

    --- a/ggml/src/ggml-backend.cpp
    +++ b/ggml/src/ggml-backend.cpp
    @@ -1862,11 +1862,14 @@ ggml_backend_sched_t ggml_backend_sched_new(
             sched->backends[b] = backends[b];
             sched->bufts[b] = bufts ? bufts[b] : ggml_backend_get_default_buffer_type(backends[b]);
             GGML_ASSERT(ggml_backend_supports_buft(backends[b], sched->bufts[b]));
 
    -        if (sched->n_copies > 1) {
    -            for (int c = 0; c < sched->n_copies; c++) {
    -                sched->events[b][c] = ggml_backend_event_new(backends[b]->device);
    -            }
    +        // allocate events for the copy actually used even when not pipelining, so the
    +        // per-split waits can be expressed as events instead of draining the backend
    +        const int n_events = sched->n_copies > 1 ? sched->n_copies : 1;
    +        for (int c = 0; c < n_events; c++) {
    +            sched->events[b][c] = ggml_backend_event_new(backends[b]->device);
             }
         }

Note: with n_copies == 1 only index 0 is ever used (`cur_copy == 0`), so the event array
stride is untouched. `ggml_backend_sched_free` already frees `events[b][c]` for all c.

### P2 - route host<->device split-input copies through the async interface

    --- a/ggml/src/ggml-backend.cpp
    +++ b/ggml/src/ggml-backend.cpp
    @@ -1742,14 +1742,28 @@ static enum ggml_status ggml_backend_sched_compute_splits(ggml_backend_sched_t s
                     } else {
                         // try async copy, but if not possible, we can still use a sync copy without synchronizing the dst backend, since we handle the synchronization here with multiple copies and events
                         // TODO: add public function to facilitate this, since applications do not have direct access to the backend interface
                         if (!split_backend->iface.cpy_tensor_async || !split_backend->iface.cpy_tensor_async(input_backend, split_backend, input, input_cpy)) {
    -                        ggml_backend_synchronize(input_backend);
    -                        if (sched->events[split_backend_id][sched->cur_copy] != NULL) {
    -                            ggml_backend_event_synchronize(sched->events[split_backend_id][sched->cur_copy]);
    -                        } else {
    -                            ggml_backend_synchronize(split_backend);
    -                        }
    -                        ggml_backend_tensor_copy(input, input_cpy);
    +                        const bool src_host = ggml_backend_buffer_is_host(input->buffer);
    +                        const bool dst_host = ggml_backend_buffer_is_host(input_cpy->buffer);
    +                        if (!src_host && dst_host) {
    +                            // device -> host readback: keep it on the source stream so it stays
    +                            // ordered behind the producer, then wait once for the copy
    +                            ggml_backend_synchronize(split_backend);
    +                            ggml_backend_tensor_get_async(input_backend, input, input_cpy->data, 0, ggml_nbytes(input));
    +                            ggml_backend_synchronize(input_backend);
    +                        } else if (src_host && !dst_host) {
    +                            // host -> device upload: order it behind the previous use of the
    +                            // destination on the destination stream, do not block the host
    +                            if (sched->events[split_backend_id][sched->cur_copy] != NULL) {
    +                                ggml_backend_event_wait(split_backend, sched->events[split_backend_id][sched->cur_copy]);
    +                            } else {
    +                                ggml_backend_synchronize(split_backend);
    +                            }
    +                            ggml_backend_tensor_set_async(split_backend, input_cpy, input->data, 0, ggml_nbytes(input));
    +                        } else {
    +                            ggml_backend_synchronize(input_backend);
    +                            if (sched->events[split_backend_id][sched->cur_copy] != NULL) {
    +                                ggml_backend_event_synchronize(sched->events[split_backend_id][sched->cur_copy]);
    +                            } else {
    +                                ggml_backend_synchronize(split_backend);
    +                            }
    +                            ggml_backend_tensor_copy(input, input_cpy);
    +                        }
                         }
                     }
                 }

Both patches are text and unapplied. P2 depends on P1 for the non-blocking destination wait;
without P1 the event test is false and P2 falls back to `ggml_backend_synchronize(split_backend)`,
which is still correct, only not cheaper.

--------------------------------------------------------------------------------
## 9. Decision recommendation

1. Accept the negative finding: the assignment's named site (ggml-backend.cpp:1673-1674) is
   prefill only. Do not build a decode experiment on it. This alone closes the branch with
   evidence and saves a run.
2. The decode-relevant equivalent is ggml-backend.cpp:1745-1752 with the dead event paths.
   That is a legitimate, never-optimized host-serialization sub-block, but its removable share
   is bounded by roughly the synchronize API time (about 4.9% of the twin window as an upper
   bound) and is realistically about 1-4% of the step.
3. If Igor wants a decode experiment, EXP-057 should be Candidate A + B, decided by the
   model-free gates first. If the gates pass, a single paired decode check (subject to rule 1)
   decides ACCEPT/REJECT against the 2% threshold.
4. If the target is the full 24-28%, this site is not the dominant term. The dominant terms
   are the 4559 individual launches (CUDA-graph direction, closed) and the CPU barriers /
   expert kernels. That scoping should be recorded before spending a run here.
