# P6-02 investigation: CUDA-split abort on a long prompt

Own investigation. No external consultation: the defect is diagnosed and fixed
inside this repository. This note records what is measured, what is ruled out and
the next instrument.

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

Diagnostic plan (own work, in order):

1. Instrument `ggml_backend_sched_split_graph` in a diagnostic base to log every
   split's `backend_id`, its node names and the cause tag, under `-ngl 0` with the
   long prompt. Diff that log against the flint base on the same request.
2. If a split is assigned CUDA0, print `sched->n_backends`, each backend's name and
   device at `ggml_backend_sched_new`, and the `backend_from_buffer` result for the
   offending node. `-ngl 0` should leave only CPU and (maybe) an unused CUDA
   backend; the assignment order is the suspect.
3. Check whether the Goose scheduler hooks (`fg164_*`) or the expert-pipeline
   prologue run on this request at all: with the pipeline disabled they must be
   inert, and a diagnostic counter proves it.
4. Reduce to a scheduler-only C harness: one CPU source tensor, one copy node,
   `ggml_backend_sched_graph_compute` with a CUDA backend registered but untasked.
   If that reproduces, the fix belongs in the scheduler object; if not, the trigger
   is earlier in llama.cpp graph construction and the harness must mirror it.

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

Open, worked locally. The next instrument is the split-assignment log described
above; the scheduler-only harness follows from what it shows.
