# EXP-057 dead levers — closed without runs

Scope: environment knobs that cannot change throughput on the accepted EXP-046 runtime
on this machine, with the code/build mechanism that proves it. Closing these without a
model run is the gate-G5 policy; the closure commit waits for Igor's confirmation.

Accepted runtime: `build/exp046-default-runtime/bin/llama-server`
(sha256 `3b109f0f80e56e4b6631eba8cee451ea89b0cfe16e17ea0383cc324e5090c2ec`), source
tree `work/llama.cpp-exp046-default`, GPU RTX 4070 (Ada, sm_89), OpenMP build.

## 1. GGML_ET_UBERKERNEL / GGML_ET_PROFILE / GGML_ET_KERNELS_PATH

Mechanism: the `ggml-et` backend is not compiled into the accepted build.

- `ggml/CMakeLists.txt:260` — `option(GGML_ET "ggml: use ET backend" OFF)`.
- `build/exp046-default-runtime/CMakeCache.txt` — `GGML_ET:BOOL=OFF`.
- Consequence: every `GGML_ET_*` variable is dead code; it cannot be set to a value
  that changes behaviour because the code path is not linked into `llama-server`.

## 2. GGML_CUDA_PDL

Mechanism: programmatic dependent launch is gated on Hopper (sm_90); the build targets
sm_89 (Ada).

- `ggml/src/ggml-cuda/common.cuh:126` and `:132` — `#if defined(GGML_CUDA_USE_PDL) &&
  defined(__CUDA_ARCH__) && __CUDA_ARCH__ >= GGML_CUDA_CC_HOPPER`.
- `ggml/src/ggml-cuda/common.cuh:1672` — `const bool can_use_pdl = attr.ptxVersion >= 90;`.
- Consequence: even if `GGML_CUDA_PDL` is set, `can_use_pdl` is false for sm_89
  kernels, so PDL never activates on the RTX 4070.

## 3. --poll

Mechanism: the polling loop is compiled only when OpenMP is disabled, and the accepted
build uses OpenMP.

- `ggml/src/ggml-cpu/ggml-cpu.c:3440` — `#ifndef GGML_USE_OPENMP` opens the block that
  contains the polling helpers, including `ggml_graph_compute_poll_for_work`
  (`:3472`).
- Consequence: `--poll` / `--poll-batch` / `--poll-draft` are no-ops in this build; the
  OpenMP team-barrier path does not consult them.

## 4. --spec-draft-cpu-mask (and the other draft CPU-mask variants)

Mechanism: the MTP draft context is created without an attached threadpool, and the
draft parameter propagation copies only thread counts, not the CPU mask.

- `tools/server/server-context.cpp:1133` — `spec_init =
  common_speculative_init_from_params(params_dft, model_tgt, ctx_tgt);` then
  `:1135` — `ctx_dft = spec_init->context();`. This path does not call
  `common_init_from_params`, which is the only place that attaches threadpools.
- `common/common.cpp:1432` — `pimpl->threadpools.init(lctx, params);` (target context
  only); `common/common.cpp:1859` — `llama_attach_threadpool(ctx, threadpool,
  threadpool_batch);` (target context only).
- `common/speculative.cpp:2517-2519` — only `n_threads` is propagated
  (`result.cpuparams.n_threads`, `result.cpuparams_batch.n_threads`); no `cpumask`
  field is copied.
- Consequence: the draft runs on a disposable threadpool with default (empty) affinity,
  so `--spec-draft-cpu-mask` has no effect. Note: `-td` / `-tbd` (thread counts) do
  work because they are propagated as `n_threads` (consistent with EXP-047).

## Conclusion

These four groups (6 individual knobs) are closed as `CLOSED_NO_RUN`; no model runs are
allocated to them. A future change of build (ET backend enabled, Hopper GPU, non-OpenMP
build, or a server path that attaches a threadpool to the draft context) would
invalidate this closure and must reopen the corresponding knob.
