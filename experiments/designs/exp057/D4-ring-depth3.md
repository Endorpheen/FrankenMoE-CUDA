# D4 - Pinned upload ring, depth 3 (general depth-N): design, correctness, memory, tests, A/B, patch

Status: DESIGN ONLY. No build, no model, no server, no profiler was run for this document.
Target tree inspected read-only: `/home/end0/AI/FrankenMoE-CUDA/work/llama.cpp-exp046-default`
(dirty checkout of the `expert-tier` pin `4aaad5d318a790a42c2197975ec8fadbad42602b` with the
integration, MTP, pinned-ring, pinned-ring-default-on and two-thread-gather patches applied).

Source hashes captured during this analysis:

- `ggml/src/ggml-cuda/common.cuh`  sha256 `26649d2a819390ee8723e2655dcd24a6a5d70c63b80d7269c5ae24a5d9ea17ec`
- `ggml/src/ggml-cuda/ggml-cuda.cu` sha256 `54cad7669e6b4ea2b9befa83181e023d9c025da5ea6a3cf0b02938033084f4ae`
- `results/archive/EXP-2026-09-07-041/tests/test-exp041-ring.cpp` sha256 `eca1a03f1b2cabde388567567531d6e046f9cbe9cc0eb3277da89616e08170c5`

Both patches below were generated from temporary copies and verified with
`git apply --check` and `patch -p1 --dry-run` on copies under `/tmp/exp057-designs/` only.
No repository file was modified.

---

## 0. Hypothesis and headline finding

Hypothesis (single, falsifiable): a third 16 MiB pinned slot increases the ring pipeline depth
from one chunk of CPU lead to two, so part of the 326.679 ms `expert_slotwait` union observed in
EXP-043 (inside which `cudaEventSynchronize` is 321.093 ms) disappears and CPU fill / H2D overlap
improves, reducing bulk-prefill latency. Accept gate is the repo decision rule: median prefill
gain >= 3% with >= 4/5 paired wins and no unacceptable regression.

Headline finding of the code review: the ring is already written depth-agnostic. Slot arrays,
allocation loops, teardown loops and the slot-cursor modulo are all expressed in terms of
`GGML_CUDA_EXPERT_RING_SLOTS`. For N=3 the entire functional change is one constant. There is no
free-list, there is exactly one gather helper (not one per slot), and per-slot events already
exist. EXP-038 declined the third slot for budget reasons, not for correctness or complexity
reasons: the accepted pinned budget was fixed at 2 x 16 MiB = 32 MiB, and a third slot raises it
to 48 MiB.

Consequence: the A/B is cheap to run but the upside is bounded by the part of the slot-wait union
that is actually removable when the measured bottleneck is the CPU gather (EXP-043: gather 1463.641
ms, 56.9% of the window). See section 9 for why this may well reject.

---

## 1. Current implementation, exact locations

All line numbers are for the two target files above.

### 1.1 Constants and context fields (`common.cuh`)

```
1417: static constexpr size_t GGML_CUDA_EXPERT_RING_SLOTS      = 2;
1418: static constexpr size_t GGML_CUDA_EXPERT_RING_SLOT_SIZE  = 16 * 1024 * 1024;
```

Context fields (`common.cuh:1420-1433`):

```
1423: cudaEvent_t copy_event = nullptr;
1425: // pinned host ring for staging MoE expert weights, owned by this context
1426: void * expert_ring_slots[GGML_CUDA_EXPERT_RING_SLOTS] = { nullptr };
1427: cudaEvent_t expert_ring_events[GGML_CUDA_EXPERT_RING_SLOTS] = { nullptr };
1428: bool expert_ring_pending[GGML_CUDA_EXPERT_RING_SLOTS] = { false };
1429: size_t expert_ring_next = 0;
1430: // 0 = not initialized, 1 = active, -1 = disabled (alloc failed, stay on the pageable path)
1431: int expert_ring_state = 0;
1432: size_t expert_ring_n_calls = 0;
1433: size_t expert_ring_n_chunks = 0;
```

Gather-helper fields (`common.cuh:1442-1455`), added by `patches/two-thread-gather.patch` and
placed after `curr_stream_no`:

```
1445: std::thread gather_helper;
1446: std::atomic<uint64_t> gather_seq{0};
1447: std::atomic<uint64_t> gather_done{0};
1448-1449: atomic futex words
1450: std::atomic<uint32_t> gather_quit{0};
1451: const uint8_t * gather_src = nullptr;
1452: uint8_t * gather_dst = nullptr;
1453: size_t gather_head = 0;
1454: size_t gather_tail = 0;
1455: int gather_helper_state = 0;
```

### 1.2 The slot state machine and chunking (`ggml-cuda.cu:2607-2679`)

State machine per slot: `FREE -> CPU_FILL -> H2D_IN_FLIGHT -> FREE`.

```
2627: while (remaining > 0) {
2628:     const size_t chunk = std::min(remaining, GGML_CUDA_EXPERT_RING_SLOT_SIZE);
2629:     const size_t slot_idx = cuda_ctx->expert_ring_next;
2630:     cuda_ctx->expert_ring_next = (cuda_ctx->expert_ring_next + 1) % GGML_CUDA_EXPERT_RING_SLOTS;
2632:     // the slot must not be refilled before its in-flight H2D has completed
2633:     if (cuda_ctx->expert_ring_pending[slot_idx]) {
2634:         CUDA_CHECK(cudaEventSynchronize(cuda_ctx->expert_ring_events[slot_idx]));
2635:         cuda_ctx->expert_ring_pending[slot_idx] = false;
2636:     }
2637:     if (chunk >= GGML_CUDA_EXPERT_RING_GATHER2T_MIN_CHUNK && ggml_cuda_gather_helper_start(cuda_ctx)) {
             ... two-thread gather: helper copies tail, caller copies head, caller waits for gather_done ...
2664:     } else {
2665:         memcpy(cuda_ctx->expert_ring_slots[slot_idx], src + done, chunk);
2666:     }
2667:     CUDA_CHECK(cudaMemcpyAsync((char *) tensor->data + offset + done,
2668:                                cuda_ctx->expert_ring_slots[slot_idx],
2669:                                chunk, cudaMemcpyHostToDevice, cuda_ctx->stream()));
2670:     CUDA_CHECK(cudaEventRecord(cuda_ctx->expert_ring_events[slot_idx], cuda_ctx->stream()));
2671:     cuda_ctx->expert_ring_pending[slot_idx] = true;
2672:     done += chunk; remaining -= chunk;
2673: }
```

Capture guard (`ggml-cuda.cu:2613-2618`) runs before ring init and returns false when
`cudaStreamIsCapturing` reports any state other than `cudaStreamCaptureStatusNone`.

### 1.3 Init, disable, teardown

- Init (`ggml-cuda.cu:2573-2603`): allocates `GGML_CUDA_EXPERT_RING_SLOTS` slots with
  `cudaHostAlloc(..., cudaHostAllocDefault)`, then `GGML_CUDA_EXPERT_RING_SLOTS` events with
  `cudaEventCreateWithFlags(..., cudaEventDisableTiming)`. On any failure it calls
  `ggml_cuda_expert_ring_disable` and returns false (all-or-nothing).
- Disable (`ggml-cuda.cu:2548-2564`): joins the gather helper first (2550), then loops the slots
  destroying events and freeing host memory (2551-2560), sets `expert_ring_state = -1`.
- Destructor (`ggml-cuda.cu:706-726`): joins the helper (711) before touching slots; logs counters
  (714-716); loops the slots with `cudaEventSynchronize` (drain), `cudaEventDestroy`,
  `cudaFreeHost` (717-726). Streams/handles are torn down after this block.
- The gather helper is single: `ggml_cuda_gather_helper_run` (`ggml-cuda.cu:2489-2510`) services one
  job at a time, gated by `gather_seq`; `ggml_cuda_gather_helper_start` (2519-2536) is lazy;
  `ggml_cuda_gather_helper_stop` (2539-2546) is idempotent and is called from both disable and the
  destructor.

### 1.4 Dispatch

- Interface member declared in `ggml/src/ggml-backend-impl.h:144`.
- Scheduler gate and call in `ggml/src/ggml-backend.cpp:1690-1717`, bounded by
  `ids_tensor->ne[1] >= 8` and a non-null `set_tensor_async_pinned_ring`; returning false falls
  back to `ggml_backend_tensor_set_async`.
- Backend hookup at `ggml-cuda.cu:4830`.

A tree-wide grep shows `GGML_CUDA_EXPERT_RING_SLOTS` is referenced only in `common.cuh` and
`ggml-cuda.cu`, so the change surface for depth is exactly those two files.

---

## 2. What changes for N slots

| Item | Change for N slots | Why |
| --- | --- | --- |
| Array sizing | None beyond the constant. Arrays are `[GGML_CUDA_EXPERT_RING_SLOTS]`. | `common.cuh:1426-1428` |
| Slot selection / free-list | None. There is no free-list; selection is the round-robin cursor `expert_ring_next`, advanced by 1 and taken modulo `GGML_CUDA_EXPERT_RING_SLOTS` (`ggml-cuda.cu:2629-2630`). With N slots the gap between two uses of the same slot is exactly N chunks. | round-robin generalizes trivially |
| Per-slot events | Already present: `expert_ring_events[N]` and `expert_ring_pending[N]`. Init, disable and destructor loops are constant-driven. | `common.cuh:1427-1428`, `ggml-cuda.cu:2587-2598`, `2551-2560`, `717-726` |
| Gather helper per slot | Not needed and not added. One helper with one in-flight job (`gather_seq`) is independent of slot count. The caller always joins both halves before the H2D, and a slot is refilled only after its own event completed, so the helper can never write a slot that is being read by an in-flight H2D. | `ggml-cuda.cu:2479-2546`, `2633-2663` |
| Chunking | None. `chunk = min(remaining, SLOT_SIZE)`; a chunk always fits exactly one slot. | `ggml-cuda.cu:2628` |
| Counters | None. `expert_ring_n_calls` and `expert_ring_n_chunks` are independent of N. | `ggml-cuda.cu:2676-2677` |
| Teardown | None. Destructor and disable loop over the constant; all allocated slots are drained/freed. | `ggml-cuda.cu:717-726`, `2551-2560` |
| Log | None. The init WARN already prints `GGML_CUDA_EXPERT_RING_SLOTS`. | `ggml-cuda.cu:2600-2601` |

Net functional change: the single constant plus two stale-comment fixes. The patch is in section 8.

Optional (not part of the minimal patch, and gated as experiment-only) general depth-N knob for a
model-free 1/2/3/4 sweep: replace the array size with a `MAX` constant, add a runtime
`expert_ring_depth` field defaulting to 3, replace the constant in the two init loops, the
destructor/disable free loops (use MAX to also free partially initialized sets), and the modulo,
and read an env override clamped to `[1, MAX]`. This changes more lines and adds an opt-in knob
that rule 10 discourages for delivery, so it is intentionally excluded from the minimal patch.
The A/B does not need it: the single variable is the constant in two otherwise identical builds.

---

## 3. Correctness argument for N slots

### 3.1 Bytes cannot corrupt

For each chunk the ordering is fixed: wait for the slot's own event (2633-2636), CPU fill into the
slot (2665 or the two-thread path), then enqueue the H2D (2667-2669), then record the event
(2670), then mark pending. A slot is refilled only after its previous event completed, so the CPU
never overwrites bytes while the H2D of that slot is still reading them. The two-thread gather
writes `gather_dst`, which is set to the current slot only after the wait; the caller blocks until
`gather_done == seq` before the H2D (2651-2663). Changing N changes only which one of N buffers
holds the bytes; it does not change any of these transitions.

### 3.2 Same-stream H2D cannot be reordered before the MMQ consumer

All chunks of every staged range are enqueued with `cudaMemcpyAsync(..., cuda_ctx->stream())`
(2667-2668) on the single compute stream, in strictly increasing `done` order. The stream is
FIFO: chunk k is enqueued before chunk k+1, and every later kernel on the same stream (including
the MMQ consumer) is ordered after all of them. Destinations are disjoint and monotonic:
`(char *) tensor->data + offset + done` with `done` increasing by `chunk`. Adding slots does not
add a stream, does not reorder the enqueue loop, and does not change destination offsets. The
EXP-043 trace confirms the property the design relies on: H2D/kernel intersection on this stream
is exactly 0 ms.

### 3.3 Sentinel and padding invariants

The scheduler computes `size = expert_size_copy + padding_end`, where `padding_end` is 512 bytes
for every expert except the last (`ggml-backend.cpp:1699-1707`). That whole byte count is passed to
the ring and chunked. The trailing padding is therefore copied as ordinary payload from the
original source, exactly as the pageable path does. The ring writes only inside
`[tensor->data + offset, + size)`; it never writes the guards or the untouched experts. The last
expert has `padding_end = 0`, so the final destination byte is the buffer end. Chunk boundaries
may fall inside an expert or inside padding, but because the chunks land at consecutive offsets on
one stream the assembled image is byte-identical. N slots do not change the arithmetic.

### 3.4 Allocation-failure fallback

Init allocates all N slots and all N events before any staged submission. If any allocation fails,
`ggml_cuda_expert_ring_disable` frees everything already allocated, clears the CUDA error, logs
once, sets `expert_ring_state = -1`, and every later call returns false, so the scheduler uses the
pageable path. With N=3 the behavior is all-or-nothing: a failed third 16 MiB allocation disables
the whole ring rather than silently degrading to 2 slots. This matches the existing contract and
keeps compute off any partially staged load. Graceful degradation to the largest successfully
allocated depth is a possible enhancement, but it changes the state machine and is deliberately
out of scope (see risks).

### 3.5 Capture / replay (CUDA graph)

The capture guard (2613-2618) runs before ring init and returns false whenever the stream is
capturing. Therefore no slot allocation, no host wait and no pinned host pointer is captured. On
replay the graph contains only the ordinary copies it captured (or none, because the call
returned false). Slot count does not affect this: the guard is unconditional and precedes all ring
state.

### 3.6 Interaction with the two-thread gather

The helper is single-job. The producer (calling thread) publishes the job fields, release-stores
`gather_seq`, copies the head itself, then acquire-waits `gather_done == seq`. Only then is the
H2D enqueued. The helper cannot be running a job for a different slot at the same time because
there is at most one in-flight job. Because a slot's refill waits on its own event, and the helper
is only invoked after that wait, the helper's write target is always a slot whose previous H2D has
completed. Hence N slots are safe with the two-thread gather. A per-slot helper is not required;
adding one would introduce a second in-flight gather job and would need its own producer/consumer
protocol, which is a larger change and a separate hypothesis.

---

## 4. Memory

- Per slot: one pinned 16 MiB buffer (`GGML_CUDA_EXPERT_RING_SLOT_SIZE = 16 * 1024 * 1024 =
  16,777,216 bytes`, allocated with `cudaHostAllocDefault`) plus one `cudaEvent_t` created with
  `cudaEventDisableTiming`.
- Depth 2 (accepted): 2 x 16 MiB = 32 MiB pinned.
- Depth 3 (proposed): 3 x 16 MiB = 48 MiB pinned.
- Delta: +16,777,216 bytes (+16 MiB) pinned, i.e. +50% over the accepted ring budget.
- Metadata delta: +1 slot pointer (8 B) +1 event handle (8 B) +1 bool (1 B, absorbed by struct
  padding). Well under 1 KiB; the dominant cost is the 16 MiB buffer. CUDA/OS per-event bookkeeping
  is not counted.
- Budget statement: the accepted EXP-038/EXP-040 fixed budget was 32 MiB. Depth 3 requires a new
  explicit budget of 48 MiB. This budget change must be stated and approved before the model run.
  The pinned region is non-pageable, so 16 MiB less pageable RAM is available; on the 64 GB test
  host this is immaterial, but it must be recorded in the run metadata.
- Measurement caveat: EXP-038 noted that CUDA pinned pages are not reliably reflected in
  `/proc/self/status` `VmPin`. Use post-request RSS delta as the practical memory gate
  (EXP-038 saw a 24732 KiB B-A delta against a 32 MiB budget), and keep `VmSwap == 0`.

---

## 5. Model-free test plan (extend the existing ring harness)

The existing harness is
`results/archive/EXP-2026-09-07-041/tests/test-exp041-ring.cpp` (with
`run-model-free-exp041.sh`). It has two modes: `ring` (direct backend-interface staging) and
`sched` (a real 395-token `MUL_MAT_ID` graph through the scheduler). It already covers byte
equality over sparse/consecutive/first/last/padding/chunk-boundary ranges, sentinel guards, the
capture guard, the alloc-fail fallback, counters, the sched output SHA across OFF/ON/alloc-fail,
and a weak VmPin teardown check.

For depth 3, copy it to `results/archive/EXP-2026-09-15-057/tests/test-exp057-ring.cpp` and make
the following changes (patch in section 8.2):

1. Mirror layout. The test's `cuda_ctx_mirror` (lines 53-65) reproduces the private context layout
   by byte offset and reads `ctxm->streams[...]`. Its arrays must become `[3]`:
   `ring_slots[3]`, `ring_events[3]`, `ring_pending[3]`. If this is missed, the mirror offset of
   `streams` is wrong and the capture test reads garbage.
2. Slot-reuse / wrap. With 3 slots and the default `EXP038_EXPERT_SIZE=4 MiB`, the six existing
   ranges produce 8 chunks, which yields only partial wraps over 3 slots. Add a seventh range
   `{6*EXP, 6*EXP + 512}` (24 MiB + 512 -> 2 chunks) so the run produces 10 chunks, and raise the
   reuse assertion from `>= 6` to `>= 9` ("at least three full 3-slot reuse wraps"). This
   guarantees each slot is reused at least twice and the cursor wraps at least three times.
3. Chunk boundary. The existing `B` range (20 MiB + 512) and the new `G` range (24 MiB + 512) both
   cross the 16 MiB slot boundary and must be split into two chunks; `want_chunks` is computed
   generically and will account for `G` automatically. Keep the byte-equality check that covers
   the boundary.
4. First and last expert. Keep `A` at offset 0 (first expert) and `F` at `13*EXP` with exact size
   to the buffer end (last expert, `padding_end = 0`).
5. Slot reuse cycles. Add a "wrap storm" phase after the main ranges: issue one single-expert
   range `k` times (for example 40 calls) and confirm `ring_n_calls` and `ring_n_chunks` increase
   by exactly that count, destination bytes stay equal to the source, and no CUDA error appears.
   Snapshot the counters before the phase so the expected deltas are exact. This stresses the
   cursor modulo and repeated event reuse beyond the 10-chunk main phase.
6. Alloc-fail. Keep `GGML_EXPERT_RING_ALLOC_FAIL=1`: the ring must log "staying on the pageable
   path", all staged calls must decline, `ring_n_calls == 0`, `ring_state == -1`. With N=3 this
   also documents the all-or-nothing behavior.
7. Teardown drain. Keep the free-path check: `ggml_backend_free(cuda)` must produce no CUDA error,
   and the VmPin delta is recorded (weak signal, as before). The destructor must drain all three
   events; a missing slot would surface as a CUDA error or a leak.
8. Capture guard. Keep the `cudaStreamBeginCapture` / `cudaStreamEndCapture` block; the ring must
   decline to stage during capture.
9. Sched mode. Keep the 395-token `MUL_MAT_ID` run and the output SHA comparison across OFF/ON/
   alloc-fail. The SHA must be identical to the accepted EXP-041 SHA
   `39505119b1...e91510`; depth changes only staging, not results.
10. Gather-mode coverage. Run the harness twice with a fixed `GGML_EXPERT_RING_GATHER2T` value
    (once `0` single-thread, once `1` two-thread) so both gather paths are checked against the
    3-slot ring. Note EXP-045 (two-thread gather) is `INCONCLUSIVE` and its code is nonetheless
    present in the exp046 tree, so the gather mode must be an explicit, recorded constant in every
    run, never left to chance.
11. Negative control. Run the depth-3 build with `GGML_EXPERT_PINNED_RING=0` and confirm the ring
    is never initialized and the pageable path is used (the existing OFF branch).

All model-free runs are on an isolated build directory, one at a time, without the server and
without the model (repo rule 1 applies to the model/server, not to these harness runs, but they
are still run serially).

---

## 6. A/B protocol (model)

Single variable: pinned slot count, 2 (A) versus 3 (B). Everything else fixed.

Builds (build-flag experiment, per the experiment lifecycle):

- Build A: `build/exp057-ring2` from the target tree with `GGML_CUDA_EXPERT_RING_SLOTS = 2`.
- Build B: `build/exp057-ring3` from the same tree with `GGML_CUDA_EXPERT_RING_SLOTS = 3`.
- Both from the same source revision, same compiler/flags, differing only in that constant. Do not
  reuse an older accepted binary as A if a same-source rebuild is possible; if the older binary is
  used, record its hash and note the binary-drift confound.

Held constant across both arms: 395-token fixed prompt, 48 generated tokens, `-c 196608`, `-np 1`,
12 threads, EHS disabled, MTP `--spec-draft-n-max 2`, `cache_prompt=false`, temperature 0, draft
CPU-MoE, fixed `GGML_EXPERT_RING_GATHER2T` value, fresh server process per run, SIGINT to the exact
server pid, one run at a time. Record the negotiated PCIe link and that no foreign GPU load is
present.

Metric: primary = server prompt-eval latency in ms for the fixed 395-token request. Secondary =
decode tok/s in the same runs (regression gate), staged calls/chunks from the teardown log,
post-request RSS delta, `VmSwap`.

Run count: 5 interleaved warm-cache pairs with alternating order (A,B then B,A), 10 fresh server
launches total. Early abort on error, OOM, `VmSwap > 0`, output mismatch, or a prefill regression
worse than 10%.

Gates (pre-registered):

- ACCEPT: median paired prefill gain >= 3%; wins >= 4/5; decode regression no worse than 2%;
  completion content byte-identical (same SHA); `VmSwap == 0` in every run; RSS delta within the
  new 48 MiB budget plus the metadata allowance.
- REJECT otherwise. A working 3-slot ring with no >= 3% gain is NOT success (repo decision rule /
  EXP-038 precedent).
- Explicitly excluded from the claim: profiler wall times. EXP-043 measured a 2572.660 ms window
  under nsys; that is a diagnostic number, not a performance claim.

Required approvals before the model block (repo rule 1): state goal, duration and number of runs;
get an explicit "yes". Also get explicit approval for the pinned-budget increase from 32 to 48 MiB
before launch. Do not run the model from this document.

---

## 7. Expected effect and its bound

From EXP-043 (measured window 2572.660 ms):

- CPU gather union 1463.641 ms (56.9%). A third slot cannot reduce this; the CPU is the stated
  bottleneck.
- GPU H2D union 1346.071 ms at 17.3 GB/s, GPU idle 793.340 ms (30.8%).
- Slot wait union 326.679 ms; nested inside it `cudaEventSynchronize` 321.093 ms (union, not
  additive with 326.679). This union is the only directly addressable component.
- GPU idle while gather is running 496.453 ms (19.3%).

The third slot lets the CPU run up to two chunks ahead: after slot k is filled and submitted, the
caller can fill slot k+1 while the GPU uploads slot k, and only then must wait for slot k-1. The
removable part is at most the portion of the 326.679 ms slot-wait that is caused by insufficient
pipeline depth, not by the CPU simply returning to a slot before any H2D could plausibly have
finished. The realistic saving is therefore a fraction of 326.679 ms (12.7% of the window), and
wall-time overlap usually converts only part of a wait into saved time. A 3% win is plausible but
not guaranteed; a null or small result is a legitimate outcome and means REJECT.

---

## 8. Patch

### 8.1 Minimal N-slots runtime change (required deliverable)

Applies to `work/llama.cpp-exp046-default`. Verified with `git apply --check` and
`patch -p1 --dry-run` on copies. Not applied to the repository.

```diff
diff --git ggml/src/ggml-cuda/common.cuh ggml/src/ggml-cuda/common.cuh
--- a/ggml/src/ggml-cuda/common.cuh
+++ b/ggml/src/ggml-cuda/common.cuh
@@ -1414,7 +1414,7 @@
     }
 };
 
-static constexpr size_t GGML_CUDA_EXPERT_RING_SLOTS      = 2;
+static constexpr size_t GGML_CUDA_EXPERT_RING_SLOTS      = 3;
 static constexpr size_t GGML_CUDA_EXPERT_RING_SLOT_SIZE  = 16 * 1024 * 1024;
 
 struct ggml_backend_cuda_context {

diff --git ggml/src/ggml-cuda/ggml-cuda.cu ggml/src/ggml-cuda/ggml-cuda.cu
--- a/ggml/src/ggml-cuda/ggml-cuda.cu
+++ b/ggml/src/ggml-cuda/ggml-cuda.cu
@@ -2569,7 +2569,7 @@
     return v == nullptr || (v[0] != '\0' && strcmp(v, "0") != 0);
 }
 
-// allocates both slots and both events before any staged submission; on failure the ring stays disabled
+// allocates all slots and events before any staged submission; on failure the ring stays disabled
 static bool ggml_cuda_expert_ring_init(ggml_backend_cuda_context * cuda_ctx) {
     if (cuda_ctx->expert_ring_state != 0) {
         return cuda_ctx->expert_ring_state == 1;
@@ -2603,7 +2603,7 @@
 }
 
 // stages one host->device copy through the bounded pinned ring: fill a free slot on the CPU
-// while the H2D of the other slot is still in flight; ranges larger than one slot are chunked
+// while the H2D of the other slots is still in flight; ranges larger than one slot are chunked
 static bool ggml_backend_cuda_set_tensor_async_pinned_ring(ggml_backend_t backend, ggml_tensor * tensor, const void * data, size_t offset, size_t size) {
     ggml_backend_cuda_context * cuda_ctx = (ggml_backend_cuda_context *) backend->context;
     ggml_backend_buffer_t buf = tensor->view_src ? tensor->view_src->buffer : tensor->buffer;
```

### 8.2 Model-free harness change (EXP-057 test, copy-then-apply)

Copy
`results/archive/EXP-2026-09-07-041/tests/test-exp041-ring.cpp` to
`results/archive/EXP-2026-09-15-057/tests/test-exp057-ring.cpp`, then apply this diff to the copy.
Verified with `patch -p1 --dry-run` on a copy.

```diff
diff --git results/archive/EXP-2026-09-15-057/tests/test-exp057-ring.cpp results/archive/EXP-2026-09-15-057/tests/test-exp057-ring.cpp
--- a/results/archive/EXP-2026-09-15-057/tests/test-exp057-ring.cpp
+++ b/results/archive/EXP-2026-09-15-057/tests/test-exp057-ring.cpp
@@ -54,9 +54,9 @@
     int device;
     std::string name;
     cudaEvent_t copy_event;
-    void * ring_slots[2];
-    cudaEvent_t ring_events[2];
-    bool ring_pending[2];
+    void * ring_slots[3];
+    cudaEvent_t ring_events[3];
+    bool ring_pending[3];
     size_t ring_next;
     int ring_state;
     size_t ring_n_calls;
@@ -124,6 +124,7 @@
         { 10 * EXP,              EXP,                    "D sparse single expert 10" },
         { 11 * EXP,              EXP,                    "E sparse single expert 11" },
         { 13 * EXP,              EXP,                    "F last expert exact to buffer end" },
+        { 6 * EXP,               6 * EXP + 512,           "G consecutive 6+pad (3-slot wrap)" },
     };
 
     bool all_staged = true;
@@ -174,7 +175,7 @@
         check(log_contains("expert ring enabled"), "flag ON: ring init logged");
         check(all_staged, "flag ON: all calls staged");
         check(ctxm->ring_n_calls == want_calls && ctxm->ring_n_chunks == want_chunks, "ring counters (calls/chunks)");
-        check(ctxm->ring_n_chunks >= 6, "at least three slot reuse cycles");
+        check(ctxm->ring_n_chunks >= 9, "at least three full 3-slot reuse wraps");
     } else {
         check(!all_staged && all_fallback, "flag OFF: every staged call declined, pageable fallback used");
         check(ctxm->ring_n_calls == 0, "flag OFF: ring never used");
```

The "wrap storm" phase from section 5.5 and the two gather-mode runs from section 5.10 are
script-level additions to `run-model-free-exp057.sh` and are described there rather than shown as
a diff.

---

## 9. Unknowns and risks

1. Central risk: EXP-043 diagnosed case 1, CPU gather-bound. A third slot does not speed up
   gather, so the achievable gain is bounded by the removable part of a 326.679 ms union inside a
   2572.660 ms window (<= 12.7%, realistically much less). A < 3% median result and a REJECT are
   plausible.
2. The two-thread gather is present in the target tree but EXP-045 was `INCONCLUSIVE` and "closed
   without porting into the accepted baseline". The intended baseline gather mode for EXP-057 must
   be confirmed with Igor; EXP-045 also observed that the two-thread mode raised slot wait
   (366.683 -> 402.284 ms), which is exactly the quantity the third slot is supposed to reduce.
   Test both modes, but A/B with one fixed mode.
3. Alloc-fail is all-or-nothing: a failed third slot disables the whole ring instead of degrading
   to depth 2. On a memory-constrained host this is a behavior change relative to depth 2 (depth 2
   would have succeeded). Decide whether graceful degradation is wanted; it is not in the minimal
   patch.
4. Budget: depth 3 raises pinned host use from 32 to 48 MiB, over the accepted fixed budget. Needs
   explicit approval and must be recorded in the run metadata.
5. Pinned memory is non-pageable; the allowed RSS delta gate must be raised accordingly and
   VmPin remains a weak signal (CUDA pinned pages are not reliably reported there).
6. `cudaEventSynchronize` latency may not fall linearly: with more lead, the CPU may still block
   on the same stream because MMQ kernels interleave between H2D copies. The third slot can only
   hide the gap up to two chunks; deeper inter-chunk gaps remain.
7. Round-robin without a free-list can still pick a pending slot while another is free. A free-list
   could reduce waits further but changes slot selection, complicates the test mirror, and is a
   separate hypothesis; not included.
8. The test's `cuda_ctx_mirror` is a fragile byte-offset reproduction of the private context. Any
   field added before `streams` invalidates it; the `[3]` update is mandatory.
9. `expert_ring_next` persists across ranges and calls, so the first chunk of a call does not
   necessarily use slot 0. Tests and any instrumentation must not assume a starting slot.
10. CUDA graph capture: unchanged by design (guard precedes init), but confirm empirically that
    the larger ring does not alter capture/replay behavior in server runs.
11. No model-free test can prove the server benefit; the A/B on the model is the only evidence that
    counts. Profiled timings must not be presented as the performance claim.
12. Slot count is compile-time; the two arms are two binaries. If any other patch differs between
    the two build directories, the single-variable property is lost. Build both from the same
    source revision.

---

## 10. References

- Ring source: `work/llama.cpp-exp046-default/ggml/src/ggml-cuda/common.cuh` (1417-1455),
  `ggml/src/ggml-cuda/ggml-cuda.cu` (706-726, 2469-2679), `ggml/src/ggml-backend.cpp` (1690-1717),
  `ggml/src/ggml-backend-impl.h` (144).
- Patches: `patches/pinned-ring.patch`, `patches/pinned-ring-default-on.patch`,
  `patches/two-thread-gather.patch`.
- Experiments: `experiments/EXP-2026-09-06-038-bounded-pinned-ring.md`,
  `experiments/EXP-2026-09-07-043-prefill-bottleneck-profile.md`,
  `experiments/EXP-2026-09-08-045-two-thread-cpu-gather.md`.
- Harness: `results/archive/EXP-2026-09-07-041/tests/test-exp041-ring.cpp`,
  `results/archive/EXP-2026-09-07-041/tests/run-model-free-exp041.sh`.
- Roadmap decision rule: `ROADMAP.md` (section "Decision rule").
- Generated patch files kept for reference: `/tmp/exp057-designs/patch-ring-depth3.patch`,
  `/tmp/exp057-designs/patch-ring-depth3-test.patch`.
