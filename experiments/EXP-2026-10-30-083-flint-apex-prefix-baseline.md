# EXP-2026-10-30-083: preserve the unified Flint/Apex prefix baseline

Status: ACCEPTED (`kind=source-preservation`) for exact source reconstruction and runtime identification. This is not performance acceptance, a production promotion, or a new optimization.

## Hypothesis and scope

The existing 2026-10-29 unified Flint/Apex prefix runtime can be preserved as a self-contained source patch against an available pinned upstream commit, including its previously unrecorded Jinja prerequisite. The patch must reproduce every source file present in the build snapshot, and the executable plus shared libraries must be identified independently.

The user requested this baseline before one bounded search for a new optimization direction. No runtime change, rebuild of model/kernel libraries, model launch, or performance measurement is part of preservation. Earlier EXP-080/081 artifacts and the production integration tree are not part of this delivery.

## Exact source and runtime

- Base repository: `https://github.com/ggml-org/llama.cpp.git`.
- Base commit: `1b44e30775dcb2f93952116b05f89e5bd797fb00` (Flint support); GitHub commit API availability verified on 2026-10-30.
- Patch: `patches/flint-apex-prefix-baseline.patch`. Apply directly to this base, not on top of EXP-081 or EXP-082.
- Reconstructed complete Git tree: `011028fe59a59ee2f2af070be02ebf319438337d`.
- Original source snapshot: `/home/end0/AI/LlamaServer/llama.cpp-flint-apex-prefix/source`.
- Original runtime directory: `/home/end0/AI/LlamaServer/llama.cpp-flint-apex-prefix/build/bin`; load its libraries with the same directory in `LD_LIBRARY_PATH`.
- Executable SHA-256: `4e7f7e2f7cc6bb865c6b2fd4865527279fe610a47b96687b41fa318fc4b8789a`.
- Server implementation SHA-256: `8a963ce35bc919880354b226f5737c6c5d3f2dc47184c291badfe8470a209168`.

The executable is byte-identical to EXP-082. The changed server implementation library distinguishes this runtime. Seven model/kernel/common libraries remain byte-identical to the working Flint libraries. See `results/exp083-flint-apex-baseline/runtime-manifest.json`; binaries and model weights remain local.

Six changed/new files relative to the pinned base:

1. `common/jinja/lexer.cpp`.
2. `tests/test-jinja.cpp`.
3. `tools/server/server-context.cpp`.
4. `tools/server/server-prefix-checkpoints.h`.
5. `tests/test-prefix-checkpoints.cpp`.
6. `tests/test-prefix-checkpoint-state.cpp`.

The unified patch includes the existing Jinja hex-escape fix, the EXP-082 SWA logic, and its recurrent-state extension. The new tests in the historical snapshot are standalone sources; they were not registered in that snapshot's unchanged `tests/CMakeLists.txt`. This preservation does not silently alter that source tree to register them.

## Architecture contract (M0 of the preserved baseline)

Hardware: Ryzen 9 5950X, RTX 4070 12 GB (`sm89`), approximately 60.7 GiB system RAM.

| Property | Apex | Flint |
| --- | --- | --- |
| Model | Apex-35B-V1.0 Q4_K_M | Flint INT4 Q4_1 |
| Architecture | hybrid `qwen35moe` | `flint` with SWA |
| Context capacity | 262144 | 131072 |
| Dense layers | GPU | GPU |
| Expert placement | first 35 MoE layers CPU, last 5 GPU | CPU MoE |
| Batch / ubatch | 4096 / 4096 | 4096 / 4096 |
| Decode / batch threads | 8 / 16 | 8 / 16 |
| KV type | Q4_0 / Q4_0 | Q4_0 / Q4_0 |
| Prefix state retained | partial recurrent state | exact SWA-window state |
| Payload budget | 2048 MiB | 256 MiB |
| Disable switch | `GGML_RECURRENT_PREFIX_CHECKPOINTS=0` | `GGML_SWA_PREFIX_CHECKPOINTS=0` |

Both profiles use one slot, Flash Attention, fit off, mmap, no speculation, native prompt-prefix matching, `--cache-ram 0` and `--ctx-checkpoints 32`. EHS is off. Apex uses CPU masks `0f0f` / `ffff`, strict placement and poll 0. Sampling remains temperature 1, top-p 0.95, top-k 20, min-p 0; Flint also uses reasoning budget 4096.

Checkpoints are saved at existing prefill boundaries. They occupy bounded RAM vectors; ordinary attention KV stays under native runtime management. No persistent KV disk tier is included. Unsupported modes bypass the extension.

## Preservation checks performed on 2026-10-30

- Audited 3867 entries in the base tree. The local snapshot omits 128 unchanged upstream files: 125 vocabulary fixtures, two agent-instruction files, and the UI env example. These are retained unchanged from the pinned base, not encoded as source deletions.
- Generated the combined six-file patch, checked and applied it to a temporary Git index with a separate temporary object directory, and compared all 3742 available snapshot files against the reconstructed tree. All matched. No original Git index, source file, runtime, or library was modified.
- Recomputed executable/library hashes and matched the saved final provenance.
- Found that the existing model-free test executable prints the older SWA-only result. It is not evidence for the current recurrent-policy test source.
- Compiled the current standalone `tests/test-prefix-checkpoints.cpp` with g++-13 into `/home/end0/AI/LlamaServer/verification083-flint-apex/test-prefix-checkpoints`, without `NDEBUG`. Result: `PASS: SWA and recurrent bounds, prefix rejection, 262K budget, eviction, disable and unsupported modes`.
- No new model-state roundtrip or generation was executed for preservation. Prior roundtrip and generation evidence is retained as historical evidence, not relabeled as a fresh check.

## Recorded results and their limits

The copied `recorded-20261029-measurements.json` contains the original development measurements. Apex 32K/24K-shared TTFT was 20.662 -> 5.716 seconds; 64K/48K-shared was 44.421 -> 12.784 seconds. These are one observation per case. They are not confidence intervals, aggregate throughput, or a new benchmark of the preserved package.

The recorded Apex full-window request evaluated 261632 input tokens and generated 128 tokens: prefill 969.85 tok/s, single-response decode 21.25 tok/s. Saved full/partial state roundtrips match bitwise for Apex and Flint. Exact state recovery does not establish quality equivalence for completed agent tasks or bitwise batch-invariant generation.

No extra runtime memory is introduced by preservation. Existing payload budgets remain 2048 MiB recurrent and 256 MiB SWA. The historical maximum recurrent payload is 2107661696 bytes. The packaging process does not establish a new process/global swap verdict. System swap may already be occupied; future runs must distinguish process swap from new global swap-out.

## Reconstruct and build separately

All llama.cpp checkouts and builds must stay under `/home/end0/AI/LlamaServer/`. Use a fresh directory; do not patch the working checkout.

```bash
git clone https://github.com/ggml-org/llama.cpp.git /home/end0/AI/LlamaServer/llama.cpp-flint-apex-repro083
git -C /home/end0/AI/LlamaServer/llama.cpp-flint-apex-repro083 checkout --detach 1b44e30775dcb2f93952116b05f89e5bd797fb00
git -C /home/end0/AI/LlamaServer/llama.cpp-flint-apex-repro083 apply --check /home/end0/AI/FrankenMoE-CUDA/patches/flint-apex-prefix-baseline.patch
git -C /home/end0/AI/LlamaServer/llama.cpp-flint-apex-repro083 apply /home/end0/AI/FrankenMoE-CUDA/patches/flint-apex-prefix-baseline.patch
cmake -S /home/end0/AI/LlamaServer/llama.cpp-flint-apex-repro083 \
  -B /home/end0/AI/LlamaServer/llama.cpp-flint-apex-repro083/build \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++-13 \
  -DGGML_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=89 -DGGML_OPENMP=ON \
  '-DGGML_CUDA_FA_QUANTS=q4_0-q4_0;q8_0-q8_0;f16-f16;bf16-bf16' \
  -DLLAMA_BUILD_SERVER=ON -DLLAMA_CURL=OFF -DLLAMA_OPENSSL=ON
cmake --build /home/end0/AI/LlamaServer/llama.cpp-flint-apex-repro083/build --target llama-server --parallel 4
```

These full-build commands are a reconstruction recipe, not a build performed in this preservation task or a promise of bit-identical compiler output. The original server-only overlay commands and relevant original CMake settings are saved separately in the evidence directory.

## Artifacts and next boundary

`results/exp083-flint-apex-baseline/` holds source provenance, runtime manifest, original build commands/options, historical measurements/provenance/ranking, and hashes of local raw evidence. Raw prompts, responses, traces, binaries and weights are not added to Git. The `.gitignore` addition only excludes a private project instruction file; that file is not part of the delivery.

After the approved commit and handoff, the next requested activity is one diagnostic pass, not an automatic performance acceptance: one existing-runtime Apex server start, at most ten minutes, three manually submitted serial 32K/128 requests (cold, 24K-shared branch, repeat branch), with Nsight Systems tracing. First quantify recurrent save/restore, transfers and CPU/GPU gaps; reject a candidate direction if its critical-path upper bound is too small. A profiler run is diagnostic and cannot replace unprofiled paired acceptance. A model launch still requires Igor's explicit approval after the purpose, duration and count have been shown. No optimizer agents or subsequent experiments are authorized by this plan.
