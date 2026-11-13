# P6-02 escalation brief: CUDA-split abort on a long prompt

Owner approved a targeted consultation with the senior helper in Codex. This is
the package to hand over: facts already measured, the exact defect, what has been
ruled out, and the specific questions.

## Reproduction

Profile: `-ngl 0 -c 2048 -np 1`, model Apex-35B-V1.0-Q4_K_M, 371 evaluated
tokens. Short prompts (5 tokens) are served correctly, the long one aborts.

    llama-server -m Apex-35B-V1.0-Q4_K_M.gguf -ngl 0 -c 2048 -np 1 ... 
    POST /completion with a ~371-token prompt

Observed: `connection closed during the request`, process aborts with

    ggml_abort <- ggml_cuda_error("invalid argument", shared-cuda.cu:114)
              <- ggml_backend_cuda_buffer_set_tensor
              <- fg163_io <- fg163_gg_function_21 <- fg163_gg_function_1
              <- ggml_backend_tensor_copy
              <- fg164_component <- ggml_backend_sched_graph_compute_async

A `-g` diagnostic base library with a breakpoint on `ggml_backend_tensor_copy`
prints the pair directly:

    COPY src=norm-0 dst=CUDA0#norm-0#0

So under `-ngl 0` the scheduler creates a split-input copy tensor in CUDA0 and
copies a CPU activation into it.

## Isolation already done

| experiment | result |
|---|---|
| our libllama + flint `libggml-base` | long prompt served, zero CUDA errors |
| our libllama + our `libggml-base` | aborts as above, same `libggml-cpu`/`libggml-cuda` |
| `CUDA_VISIBLE_DEVICES=""` | long prompt served: the abort needs a CUDA backend to exist |
| every Goose switch off (`GGML_GOOSE_SCHEDULER=0`, `GGML_GOOSE_CPU_BACKEND_API=0`, `GGML_GOOSE_BACKEND_BUFFERS=0`, `GGML_GOOSE_STREAM_CONTROL=0`, `GGML_GOOSE_ALLOCATOR=0`, `GGML_EXPERT_PINNED_RING=0`, `GGML_GOOSE_EXPERT_PIPELINE=0`) | still aborts with two CUDA errors |
| `LD_PRELOAD` interposer on `ggml_backend_tensor_copy` | never fires; the call is bound locally (`-Bsymbolic`) |
| replacing the base with flint's | fixes the abort but costs 28.6 s vs 22.8 s (~25 %, far beyond the 2 % rule) |

## What is textually identical to flint

Split passes 2-4, `compute_splits`, `tensor_alloc`, both `reserve` functions,
`ggml_backend_tensor_copy`, `ggml_backend_sched_backend_from_buffer` and the
`hv_tensor_copies` sizing are line-identical once the hook lines are stripped.
The whole-file diff is 253 changed lines out of about 2 600.

## The specific unknown

Our replacement scheduler is a newer revision than the code it replaces: it uses
`sched->expert_pipeline` in 16 places, while the flint `ggml-backend.cpp` in this
build has no such field. The pipeline pays for itself (that is why flint's base is
25 % slower), so it cannot simply be dropped.

Questions for the senior helper:

1. In the newer scheduler revision, which pass assigns `backend_id` to a
   split-input copy under a graph whose only device is the CPU? The observed
   destination is `CUDA0`, so the candidate is the pass that consults the
   *destination* buffer's `buft` for a copy node rather than the source's.
2. Is `expert_pipeline` supposed to be inert when `ggml_backend_sched_set_expert_pipeline`
   is never called, or does its presence change the copy-node allocation path in
   this revision?
3. What is the minimal C-level reproducer that keeps the abort? A scheduler-only
   graph with one CPU source tensor and one copy node whose `buffer` is a CUDA
   buffer would settle whether the fault needs llama.cpp at all.

## Artifacts to inspect

- deployed base: `runtime/baseline-artifacts/libggml-base.so.0.e213fa19`
  (sha256 `e213fa19…`; preservation note in
  `docs/goose-migration/P4-03-REPRODUCIBILITY.md`)
- scheduler replacement source:
  `experiments/EXP-2026-11-08-164-goose-scheduler/source/backend-scheduler.cpp`
- build recipe: `experiments/EXP-2026-11-08-164-goose-scheduler/build-commands.json`,
  label `scheduler-base-link`
- probe evidence: `experiments/EXP-2026-11-12-304-p6-03-probe/README.md`,
  `goose.log`, `native.log`
- isolation arms: `experiments/EXP-2026-11-12-305-sched-isolation/`
- diagnostic build and gdb script: `experiments/EXP-2026-11-12-307-sched-debug/`

## What the fix must satisfy

- the 371-token request is served exactly like the flint base line
  (`c5ca940bbe49eaa5` for the 8-token continuation);
- the Apex acceptance keeps `e68df05b`, exact ids and probabilities;
- decode stays within the 2 % rule of the accepted 41.62 / 43.04 tok/s and VRAM
  does not grow beyond the recorded figure.

## Status

Blocked pending senior-helper guidance. The minimal reproducer from question 3 is
not yet isolated to a scheduler-only C harness; that is the next instrument.
