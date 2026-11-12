# EXP304: graph-shape probe found a live copy-path defect

The probe was meant to compare graph shapes between lines by running two prompt lengths (5 and 371 evaluated tokens) on
both. The native line returned both answers; our line returned the short one and then died on the long one, which is a much
more important result than the comparison.

## Result

| arm | short prompt (5 tokens) | long prompt (371 tokens) |
|---|---|---|
| native | `1da0f49942ac7858`, " over the lazy dog." | `c5ca940bbe49eaa5` |
| ours | `1da0f49942ac7858`, identical | **connection closed**, process aborted |

## Where it dies

    #5  ggml_abort
    #6  ggml_cuda_error                  libggml-cuda.so.0
    #7  ggml_backend_cuda_buffer_set_tensor
    #8  fg163_io                         libggml-base.so.0
    #9  fg163_gg_function_21
    #10 fg163_gg_function_1
    #11 ggml_backend_tensor_copy          libggml-base.so.0

with `CUDA error: invalid argument` raised from the CUDA backend at `shared-cuda.cu:114`.

Both lines run with `-ngl 0` and neither prints a CUDA device line, so nothing should be copied to a CUDA buffer at all.
The run log shows the recurrent prefix checkpoint being written before the failure:

    slot create_check: id 0 | task 0 | RAM recurrent prefix saved: tokens = 1, size = 62.813 MiB, retained = 1

## Why this matters for the ledger

P6-02 was closed minutes earlier on the short-prompt evidence, with attachment recorded as ported and live and ownership as
native-and-exercised. A second prompt shape contradicts the "exercised" half: a copy driven by fg163 reaches a CUDA buffer
and aborts. The item is reopened until this is explained and fixed; closing it again requires both shapes green.

The probe itself is otherwise sound and will be rerun once the copy path is fixed, since the graph-shape comparison it was
written for is still useful.

## Localization: the scheduler decides the copy

A breakpoint on `ggml_backend_tensor_copy` stops inside the scheduler, not the loader:

    #1 fg164_component
    #2 fg164_gg_function_84
    #3 fg164_gg_function_1
    #4 ggml_backend_sched_graph_compute_async
    #5 llama_context::graph_compute
    #6 llama_context::process_ubatch

So the copy is part of graph scheduling for the 371-token ubatch: the scheduler module (EXP164) decides a tensor needs to move,
and its destination is a CUDA buffer even though the run is `-ngl 0` and no CUDA device line is printed. The short prompt
never reaches this split, which is why the first request succeeds.

That reframes the defect: it is not weight placement (P6-02's attachment decision) and not the recurrent checkpoint, but the
scheduler's backend assignment for a split graph. The next step is to see what fg164 returns for that node -- a valid backend
index or something out of range -- rather than to guess from the tensor side.

## Isolation: our libggml-base is the faulty half

The same reproducer with **our libllama and the flint libggml-base** answers the long prompt and logs zero CUDA errors. With
our libggml-base it aborts. So the fault is the scheduler replacement in our base library, not the model, not the loader, not
the KV path.

Two further facts narrow it:

- Every switch tried (`GGML_GOOSE_SCHEDULER=0`, `GGML_GOOSE_CPU_BACKEND_API=0`) still aborts, so this is not a hook decision
  -- the replacement file itself behaves differently.
- `ggml_backend_tensor_copy` and `ggml_backend_sched_backend_id_from_cur` are textually identical to the flint originals, so
  the divergence is not in those two functions.
- The replacement is not the same revision as the code it replaces: our `backend-scheduler.cpp` uses `sched->expert_pipeline`
  in 16 places, while the flint `ggml-backend.cpp` that ships in the build has no such field at all. A port that swaps in a
  newer scheduler cannot assume the surrounding behaviour is preserved.

Under `-ngl 0` the split that reaches CUDA should not exist. The next step is therefore to compare the split-assignment passes
(two and three) between the two revisions and find where ours assigns a CUDA backend where flint assigns the CPU one.

## What is ruled out, and the trap in reverting

Facts measured in this pass:

- **The CUDA backend must be present for the abort.** With `CUDA_VISIBLE_DEVICES=""` the same 371-token request answers and logs
  zero CUDA errors. Under `-ngl 0` the split that reaches CUDA therefore should not exist, which keeps the scheduler as the
  suspect rather than the model path.
- **The obvious algorithms are identical to flint.** Passes 2 to 4 of the split, `compute_splits`, `tensor_alloc` and both
  `reserve` functions, `ggml_backend_tensor_copy`, `ggml_backend_sched_backend_from_buffer` and the `hv_tensor_copies` sizing
  all match the flint source line for line once the hook lines are stripped. The whole-file diff is 253 changed lines out of
  about 2600, so the divergence is in the remainder.
- **Reverting the base library is not a fix.** The flint `libggml-base` does make the long prompt work and keeps Apex at
  `e68df05b`, but the accepted profile takes **28.6 s instead of 22.8 s** -- roughly 25 % slower, far beyond the 2 % rule. The
  replaced scheduler carries the expert pipeline, so it cannot simply be dropped.
- **The expert switches do not change the abort.** `GGML_EXPERT_PINNED_RING=0` and `GGML_GOOSE_EXPERT_PIPELINE=0` both still
  abort with two CUDA errors.

So the task is narrowed to the 253-line remainder: find the hunk that makes a copy land in a CUDA buffer, without touching the
pipeline that pays for itself.

## The remaining unknowns, and the next instrument

Two more probes failed to separate the cause and are recorded so they are not repeated:

- **Every Goose switch in the base library still aborts**: `GGML_GOOSE_BACKEND_BUFFERS=0`, `GGML_GOOSE_STREAM_CONTROL=0`,
  `GGML_GOOSE_ALLOCATOR=0`, `GGML_GOOSE_CPU_BACKEND_API=0`, `GGML_GOOSE_SCHEDULER=0`. So the defect is not behind any hook.
- **An `LD_PRELOAD` interposer on `ggml_backend_tensor_copy` never fires** (zero `COPY` lines) while the crash backtrace goes
  through that exact symbol, which means the call is bound locally inside `libggml-base` and cannot be intercepted from
  outside.

The base library exports only 24 symbols that flint's does not, and all of them are `fg*` counters and helpers -- no ggml
function is overridden. Together with the near-identical source, this means the difference is a *build-level* one: our
`libggml-base` is linked with an object compiled from `backend-scheduler.cpp` in place of the original scheduler object, and
the two revisions are not the same code even though most functions match.

Next instrument: compile that one object with `-g -O2` using the recorded EXP164 recipe, link a **diagnostic** base library
(not for deployment), and read the split's `backend_id` and the input tensor name at `ggml_backend_sched_copy_input`. With
types available, gdb prints the fields directly instead of my guessing offsets.

## The failing copy, seen directly

With a `-g` build of the scheduler object linked into a **diagnostic** base library, a breakpoint on
`ggml_backend_tensor_copy` reports the exact pair:

    COPY src=norm-0 dst=CUDA0#norm-0#0

So the scheduler creates a split-input copy tensor **in the CUDA0 backend** and copies a CPU activation into it, under
`-ngl 0`. The copy itself is the standard one; the destination is the surprise. The CUDA0 copy tensor is either sized for a
ring slot that was never allocated or bound to a device buffer the copy does not fit, which is why the CUDA setter answers
"invalid argument".

That also explains why every switch and every interposer failed: the defect is not in a hook, it is which backend the split
receives.

## What is left to explain

The assignment passes we compared are identical to the flint source, and the graph is built by the same libllama in both
isolation arms. That leaves one uncomfortable possibility: the **deployed** base library may be older than the source in the
experiment directory (it is named `libggml-base-scheduler-bafe3955c9ea`, a hash of its two inputs at build time). If the source
drifted after that build, the deployed object is not the code we have been reading.

The next step is therefore a plain rebuild of the base from the current source with the recorded recipe, followed by the
reproducer, the Apex gate and a timing comparison against the 22.8 s baseline.
