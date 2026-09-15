# D2 - GGML environment-knob survey

Scope: read-only source inspection of the accepted runtime tree.

- Tree: `/home/end0/AI/FrankenMoE-CUDA/work/llama.cpp-exp046-default`
- Base commit: `4aaad5d318a790a42c2197975ec8fadbad42602b` (single squashed EXPERT_TIER commit)
- Build interrogated: `/home/end0/AI/FrankenMoE-CUDA/build/exp046-default-runtime`
  - `CMAKE_BUILD_TYPE=Release`, `GGML_CUDA=ON`, `GGML_CUDA_GRAPHS=ON`, `CMAKE_CUDA_ARCHITECTURES=89`, `GGML_ET=OFF`, `GGML_ET_SYSEMU=OFF`
  - No `libggml-et.so` is produced (ET backend not linked).
- Hardware: single RTX 4070 = compute capability 8.9 (Ada, sm_89).
- Workload: 82 GB MoE, `--cpu-moe`, split MTP (`--spec-draft-n-max 2`), decode ~20.8 tok/s, prefill 1124 tokens ~8.4 s, GPU util ~16-31%.
- Command line: `scripts/run_qwen38_server.sh` -> no `--no-op-offload`, so `op_offload = true` (default).

## Table

All env vars are read once at start-up (static init or first use); they must be exported before `llama-server` starts.

| Knob | Wired? file:line | What it does | Default | Plausible effect for this workload | How to test |
| --- | --- | --- | --- | --- | --- |
| `GGML_OP_OFFLOAD_MIN_BATCH` | Yes. read `ggml/src/ggml-cuda/ggml-cuda.cu:5758`; stored `:5781`; used `:5587` via `get_op_batch_size` `:5569-5582`; scheduler consumer `ggml/src/ggml-backend.cpp:961-967` | Minimum "batch" size for the CUDA backend to accept an op whose weight lives in host (CPU) memory. Batch metric: `MUL_MAT`=ne[1], `MUL_MAT_ID`/`ROPE`=ne[2], `GET_ROWS`=always 0, else nrows. With `--cpu-moe` the expert weights are a CPU host buffer (`common/common.h:1148-1150`), so `MUL_MAT_ID` is exactly such an op. Below threshold the MoE op stays on CPU; at/above it the scheduler assigns it to CUDA and copies the host weight (expert tensor) over PCIe. | `32` | Highest-value live lever. Decode batch is 1-3 (MTP verify) so it stays on CPU either way for a threshold <=3. The 4-token prefill tail (4 < 32) is forced onto CPU today; setting the threshold to 4 can move that tail to GPU. Conversely a very large threshold (>=1024) forces all MoE prefill ops onto CPU, which is a clean probe of how much of prefill is expert H2D transfer vs CPU compute; prefill is 8.4 s and the accepted EXP-038 pinned ring specifically optimizes expert uploads, so expert transfer is a real share of prefill. Low values must not go to 1-3 or decode/MTP-verify MoE moves to GPU per token and will likely lose badly. | Env only A/B. Test `GGML_OP_OFFLOAD_MIN_BATCH=4` then `=8`, `=16` (tail/funnel tuning) and `=4096`/`=8192` (all-CPU MoE control) versus unset (32). Measure decode tok/s and bulk-prefill latency separately. Needs server restart per value. |
| `GGML_CUDA_GRAPH_OPT` | Yes. read `ggml/src/ggml-cuda/ggml-cuda.cu:4581`; impl `:4568-4700`; hook `:4828`; invoked `ggml/src/ggml-backend.cpp:1472` | Enables an extra graph-rewrite pass on the CUDA backend. Requires `USE_CUDA_GRAPH` (defined because `GGML_CUDA_GRAPHS=ON`, `common.cuh:1229-1231`), CUDA graphs enabled, and single GPU. It finds `attn_norm` fork nodes with fan-out exactly 3 and interleaves the Q/K/V branches across streams to overlap them (`:4637-4659`). Only touches the GPU (dense) subgraph. | Disabled (must be exactly `1`; `env != nullptr && atoi(env) == 1`) | Low-to-medium. Decode is CPU MUL_MAT_ID-bound and GPU util is 16-31%, so overlapping QKV on the GPU cannot move the decode bottleneck. During prefill attention is a larger share, so a small prefill gain is conceivable. It is an experimental pass; a regression is possible. Note: with `--cpu-moe` the MoE `MUL_MAT_ID` ops sit in a CPU split, so the GPU subgraph likely has no `MUL_MAT_ID` and CUDA graphs are not blocked by the sync fallback (`:2797-2807`). | Env only A/B: `GGML_CUDA_GRAPH_OPT=1` vs unset, same runs, check decode and prefill; also gate on byte-identical output. |
| `GGML_CPU_DISABLE_FUSION` | Yes. read `ggml/src/ggml-cpu/ggml-cpu.c:4192`; used `:3329`, `:3337` | Disables the CPU RMS_NORM+MUL F32 fusion in `ggml_cpu_try_fuse_ops`. | `0` (fusion ON) | Zero / negative as a lever. The 82 GB workload is dominated by quantized IQ2_S/IQ4_NL vector-dot kernels, not F32 RMS_NORM+MUL. Setting it can only remove a CPU optimization; it cannot speed anything up. | Not worth an A/B for speed. Only useful as a correctness/isolation control. |
| `GGML_CUDA_DISABLE_FUSION` | Yes. read `ggml/src/ggml-cuda/ggml-cuda.cu:3516`; impl `ggml_cuda_try_fuse` `:3514+` (topk-MoE fusion, gated_delta_net cache fusion, mul_mat fusion) | Disables all CUDA op fusion. | `0` (fusion ON) | Zero / negative. GPU is underutilized and the fused GPU ops are dense-path only; disabling them can only add kernel launches and slow the GPU side. Not a speed lever. | Not worth an A/B for speed. Isolation control only. |
| `GGML_CUDA_PDL` | Read yes `ggml/src/ggml-cuda/common.cuh:1691`; compile gate `:120-123`; activation gate `:1622-1674`, use `:1688-1701`; device primitives `:125-135` | Programmatic Dependent Launch: launches kernels with `cudaLaunchAttributeProgrammaticStreamSerialization` to overlap back-to-back kernels. | "Enabled" when unset (`env == nullptr || atoi != 0`), but effectively a no-op on this GPU | Dead on this hardware. `ggml_cuda_kernel_can_use_pdl` requires `attr.ptxVersion >= 90` (`:1672`); the build targets sm_89 (`CMAKE_CUDA_ARCHITECTURES=89`) and the RTX 4070 is CC 8.9, so PTX version is 89 < 90 and every kernel is rejected. The device primitives are also compiled out below Hopper (`__CUDA_ARCH__ >= 900`, `:126`, `:132`). PDL needs Hopper-or-newer silicon; the 4070 cannot use it. Setting `GGML_CUDA_PDL=0` changes nothing. | Do not test. Cannot help without a Hopper/Blackwell GPU. |
| `GGML_ET_UBERKERNEL` | Read yes `ggml/src/ggml-et/ggml-et.cpp:1758`; used `ggml-et-kernels.cpp:440,458` | Enables the ET-SoC accelerator uberkernel (device-side dispatch/fusion) on the Esperanto ET backend. | Off (`env && env[0] && strcmp(env,"0")`) | Dead code in this tree/build. `ggml/CMakeLists.txt:260` has `option(GGML_ET ... OFF)`; the accepted build cache shows `GGML_ET=OFF`, no `libggml-et.so` exists in `build/exp046-default-runtime`. It targets a different accelerator, not CUDA/CPU. | Do not test. Backend not compiled. |
| `GGML_ET_PROFILE` | Read yes `ggml/src/ggml-et/ggml-et.cpp:163-186`; used `:465` | Starts an ET runtime JSON profiler writing `et_runtime_trace.json`/`kernel_id.json` under the given path. | Off (unset) | Dead code in this tree/build (ET backend off). Also purely instrumentation; it cannot speed up inference even when the backend is present. | Do not test. |
| `GGML_ET_KERNELS_PATH` | Read yes `ggml/src/ggml-et/ggml-et-kernels.cpp:147-159` | Selects where ET device kernels are loaded from (`.elf` files) instead of the embedded copies. | Embedded kernels (unset) | Dead code in this tree/build (ET backend off). Loader path only; no throughput effect. | Do not test. |
| `GGML_SCHED_DEBUG` | Read yes `ggml/src/ggml-backend.cpp:1821-1822`; used `:1009`, `:1429`, `:1569-1581` | Enables scheduler debug logging and realloc diagnostics (`>1` = per-node prints). | `0` | Diagnostic only. Setting it adds per-graph logging overhead; it cannot improve throughput. | Do not test for speed; diagnostics only. |
| `GGML_MMID_TRACE` | Read yes `ggml/src/ggml-cuda/ggml-cuda.cu:1943`, `:1956`, `:1962`, `:1968` | Prints the chosen CUDA `MUL_MAT_ID` dispatch path (`mmvq`/`mmq`/`mmf`/fallback-sync) to stderr. | Off (unset) | Diagnostic only, and it only fires if the CUDA `MUL_MAT_ID` path runs. With `--cpu-moe` and small batches that path is usually not taken. Setting it can only add I/O. | Use as a diagnostic to confirm whether `MUL_MAT_ID` actually reaches CUDA at a given `GGML_OP_OFFLOAD_MIN_BATCH`; not a speed lever. |

## Ranking by expected value (env-only, no code change)

Tier A - worth an env-only A/B:

1. `GGML_OP_OFFLOAD_MIN_BATCH` - HIGH. The only knob here that shifts work between the CPU MoE path and the GPU, i.e. the actual bottleneck. Sweep `4/8/16` (funnel/tail) and `4096/8192` (all-CPU control) against the default 32; measure decode and prefill separately. Env only.
2. `GGML_CUDA_GRAPH_OPT` - LOW-MEDIUM. Env only (`=1`). Touches only the GPU dense subgraph; decode is CPU-bound, so at best a small prefill effect, with regression risk. Cheap to test alongside experiment 1.

Tier B - only useful as controls / diagnostics, cannot speed anything up:

3. `GGML_MMID_TRACE` - diagnostic; useful to prove whether the tail reaches CUDA.
4. `GGML_SCHED_DEBUG` - diagnostic; overhead only.
5. `GGML_CPU_DISABLE_FUSION` - can only remove an optimization.
6. `GGML_CUDA_DISABLE_FUSION` - can only remove an optimization.

Tier C - dead / irrelevant in this build or on this GPU:

7. `GGML_CUDA_PDL` - compiled but inert on sm_89 (`ptxVersion >= 90` gate); needs Hopper+ hardware.
8. `GGML_ET_UBERKERNEL` - ET backend not built (`GGML_ET=OFF`).
9. `GGML_ET_PROFILE` - ET backend not built; instrumentation only.
10. `GGML_ET_KERNELS_PATH` - ET backend not built.

## Notes that bound the conclusions

- Because the decode bottleneck is CPU `MUL_MAT_ID` plus host serialization and the GPU is 16-31% utilized, no CUDA-only knob (`GGML_CUDA_PDL`, `GGML_CUDA_GRAPH_OPT`, `GGML_CUDA_DISABLE_FUSION`) can move decode. They can only affect the GPU dense subgraph.
- `GGML_OP_OFFLOAD_MIN_BATCH` is read at backend-registry time (`ggml-cuda.cu:5758`, inside `ggml_backend_cuda_reg`), so a server restart is required per value.
- `op_offload` must be true for the threshold to matter; it is true by default (`llama-context.cpp:3691`, `common/common.cpp:1772`). Adding `--no-op-offload` would disable the whole mechanism and is a useful negative control.
- Do not lower the threshold to 1-3: the MTP verify batch is 1-3, so decode-time MoE would move to the GPU with per-token expert transfers.

## Suggested first experiment (env only)

Two profiles against the current default in the same session, one at a time, per repo rules:

- A (tail-on-GPU): `GGML_OP_OFFLOAD_MIN_BATCH=4`.
- B (all-MoE-on-CPU control): `GGML_OP_OFFLOAD_MIN_BATCH=8192`.
- Baseline: unset (32).

Success criterion: a repeatable improvement in bulk-prefill latency without a decode regression, output byte-identical.
