# Blocker: our line aborts on a long prompt inside the scheduler, and the base library is not reproducible

Status: local agent stopped after eight meaningful attempts, per the blocker rule. Escalated to Igor for the senior helper.

## Expected

Run with `-ngl 0`, `-c 2048`, `-np 1`: a 371-token prompt is served exactly as the flint line serves it
(`c5ca940bbe49eaa5` for the 8-token continuation). With no layer offloaded the scheduler has no reason to involve CUDA.

## Actual

    connection closed during the request; process aborts
    ggml_abort <- ggml_cuda_error("invalid argument", shared-cuda.cu:114)
              <- ggml_backend_cuda_buffer_set_tensor
              <- fg163_io <- fg163_gg_function_21
              <- ggml_backend_tensor_copy
              <- fg164_component <- ggml_backend_sched_graph_compute_async
              <- llama_context::graph_compute <- process_ubatch

A `-g` build of the scheduler object with a breakpoint on `ggml_backend_tensor_copy` prints the pair directly:

    COPY src=norm-0 dst=CUDA0#norm-0#0

so the scheduler creates a split-input copy tensor in the CUDA0 backend and copies a CPU activation into it.

## What was tested

1. **Prompt shape**: 5 tokens fine, 371 tokens aborts. Deterministic.
2. **Switches**: `GGML_GOOSE_SCHEDULER=0`, `GGML_GOOSE_CPU_BACKEND_API=0`, `GGML_GOOSE_BACKEND_BUFFERS=0`,
   `GGML_GOOSE_STREAM_CONTROL=0`, `GGML_GOOSE_ALLOCATOR=0`, `GGML_EXPERT_PINNED_RING=0`,
   `GGML_GOOSE_EXPERT_PIPELINE=0` -- all still abort with two CUDA errors.
3. **Isolation by library**: our libllama with flint's `libggml-base` serves the long prompt with zero CUDA errors; with our
   `libggml-base` it aborts. Same `libggml-cpu`, same `libggml-cuda`, same `libllama`.
4. **GPU hidden**: `CUDA_VISIBLE_DEVICES=""` serves the long prompt, so the abort needs a CUDA backend to exist.
5. **Source diff against flint** (hooks stripped): passes 2-4 of the split, `compute_splits`, `tensor_alloc`, both `reserve`,
   `ggml_backend_tensor_copy`, `backend_from_buffer`, `hv_tensor_copies` sizing and the `sched_new` prologue are line-identical.
   The substantive remainder is the expert-pipeline block (102 non-blank lines), inert because nothing calls
   `ggml_backend_sched_set_expert_pipeline`.
6. **Interposition**: an `LD_PRELOAD` wrapper on `ggml_backend_tensor_copy` never fires because the library is linked `-Bsymbolic`.
7. **Rebuild from recorded sources**: produces a different artifact (`95a29c73` vs deployed `e213fa19`), still aborts, and
   returns `9a260527` instead of the accepted `1da0f499` for the 5-token prompt. So a faithful rebuild changes numerics, and the
   deployed artifact cannot be recreated from what is checked in.
8. **Deployment inputs**: the library name is `sha256(H(scheduler object) + H(goose unit))`; recomputing it from current sources
   yields `bafe3955c9ea`, the deployed name. So the EXP164 pair in the deployment is byte-identical to current sources. The
   other five Goose objects (`shared-core.o`, `shared-meta.o`, `allocator.o`, `buffers-goose.o`, `stream-goose.o`) were
   missing from disk and were rebuilt today from their recorded steps.

## Remaining hypotheses

- One of those five objects changes scheduler-visible state (buffer usage flags, `buft` identity, host-pointer detection), so a
  split that flint keeps on CPU is offered to CUDA in our build.
- The deployed bytes of those five objects are not what their sources now produce, which would also explain the napkin math of
  a faithful rebuild changing numerical output.
- A `ggml_backend_expert_pipeline_v1` provider may exist in our CUDA backend, enabling a path that the flint base has no code
  for; the enable call was not found in our trees, but the flint llama may call it in an object we did not rebuild.

## Paths

- deployed base: `runtime/reconstructed/build/stage-run/libggml-base.so.0` (`sha256 e213fa19...`)
- link recipe: `experiments/EXP-2026-11-08-164-goose-scheduler/build-commands.json`, label `scheduler-base-link`
- scheduler source: `experiments/EXP-2026-11-08-164-goose-scheduler/source/backend-scheduler.cpp`
- diagnostic build: `experiments/EXP-2026-11-12-307-sched-debug/` (`-g` object, debug library, gdb script)
- probe log and evidence: `experiments/EXP-2026-11-12-304-p6-03-probe/README.md`
- isolation stage: `experiments/EXP-2026-11-12-305-sched-isolation/`
