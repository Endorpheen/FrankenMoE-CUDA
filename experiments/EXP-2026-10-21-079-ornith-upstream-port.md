# EXP-2026-10-21-079: port Franken features to upstream 839a572a

Status: ACCEPTED as a separately built port. This does not replace the production EXP-063 runtime and does not establish a speedup over clean current upstream.

## Source and scope

Base: ggml-org/llama.cpp commit `839a572a4cdecdb80d211297b8e7d7323a6e99d2`.
Patch: `patches/ornith-upstream-839a.patch` (44 source files).
Validation: `results/ornith-upstream-839a-validation.json` includes the patch SHA-256, reconstructed source tree, file list, correctness results, and repeated model measurements. Applying the patch to a temporary index at the pinned base reproduces the complete tested source tree, including staged and unstaged source fixes.

The port carries the expert heatmap/hot-store integration, pinned staging ring, CPU fused expert path and tests, MTP sidecar support, and swap watchdog to the current APIs. The hot store remains off by default. Existing upstream MoE cache and EHS are mutually exclusive. In particular:

- Adapt the hot-store expert count to `n_expert_used_max()`.
- Gate the CPU fused window on explicit per-token prompt/decode markings in the current server batch view.
- Retain routing IDs only when heatmap/EHS collection is requested. The former unconditional output marking retained argsort buffers and increased the CUDA compute reservation by 350 MiB at ubatch 4096. The corrected reservation is 1810.03 MiB, matching clean upstream, instead of 2160.03 MiB.

The working EXP-063 binary, integration checkout, existing launchers, model weights, and other model profiles are not changed. This is a patch against the pinned upstream, not a replacement copy of upstream source.

## Reproduce in a new source/build directory

Run from the FrankenMoE-CUDA repository root. Do not apply the patch to the production checkout.

```bash
git clone https://github.com/ggml-org/llama.cpp.git work/llama.cpp-ornith-839a-repro
git -C work/llama.cpp-ornith-839a-repro checkout --detach 839a572a4cdecdb80d211297b8e7d7323a6e99d2
git -C work/llama.cpp-ornith-839a-repro apply --check ../../patches/ornith-upstream-839a.patch
git -C work/llama.cpp-ornith-839a-repro apply ../../patches/ornith-upstream-839a.patch
ulimit -c 0
cmake -S work/llama.cpp-ornith-839a-repro -B build/ornith-839a-repro -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DGGML_BACKEND_DL=OFF \
  -DGGML_CUDA=ON -DCMAKE_CUDA_HOST_COMPILER=/usr/bin/g++-13 \
  -DCMAKE_CUDA_ARCHITECTURES=89-real -DGGML_NATIVE=ON -DGGML_OPENMP=ON \
  -DGGML_CCACHE=OFF \
  '-DGGML_CUDA_FA_QUANTS=q4_0-q4_0;q8_0-q8_0;f16-f16;bf16-bf16' \
  -DLLAMA_BUILD_TESTS=ON -DLLAMA_BUILD_EXAMPLES=OFF \
  -DLLAMA_BUILD_SERVER=ON -DLLAMA_CURL=OFF
cmake --build build/ornith-839a-repro \
  --target llama-server test-backend-ops test-iq2s-fused-mmid --parallel 4
```

The CUDA architecture and host compiler above reproduce the tested RTX 4070 build. Select suitable values on other hardware.

## Recorded checks

No new model runs were made while packaging this commit. The following checks refer to the already completed overnight build and measurements:

- CPU fused test: ALL PASS with `GGML_CPU_IQ2S_FUSED=0` and with `=1`.
- CPU `MUL_MAT_ID`: 340/340 passed.
- CUDA `MUL_MAT_ID`: 337/337 passed.
- Operator coverage: IQ2_XS, IQ2_XXS, IQ2_S, Q4_K, Q8_0, and F16, expert counts 4 and 8.
- Model: Ornith-1.5-122B AD-IQ2_XS-IQ2_XXS; 64000 context; warmup followed by two identical 8192-input/256-output requests; Ryzen 9 5950X, RTX 4070 12 GB, 64 GB RAM; EHS off; CPU experts; Flash Attention; Q4_0 KV; 8 decode and 16 batch threads.

| Runtime/profile | Prefill tok/s | Decode tok/s |
| --- | ---: | ---: |
| Clean upstream, batch/ubatch 4096 | 1198.52-1199.85 | 17.91-18.07 |
| Corrected port, batch/ubatch 4096 | 1166.52-1169.93 | 17.42-17.56 |
| Corrected port, batch/ubatch 2048 | 846.26-848.13 | 17.48-17.54 |
| Corrected port, 1664 MiB expert cache, ubatch 512 | 326.80-327.94 | 19.54-19.85 |

The port matches the clean upstream compute-buffer size and fits the previously failing batch-4096 workload after the routing-ID fix. The table shows no performance advantage over clean current upstream. The large prefill improvement over older small-batch profiles must not be attributed to a new Franken optimization alone.

## Limits

Model-level checks cover Ornith with EHS disabled. This port does not establish current-upstream model coverage for EHS, separate MTP sidecars, or Qwen/Next. The IQ2_S fused CPU path does not accelerate Ornith's IQ2_XS/IQ2_XXS expert tensors. Separate compile-time FORCE_MMQ/FORCE_CUBLAS experiments are not enabled by this patch. Full-night safety and discarded-run history remain in the local overnight report; early failures included a system coredump disk-full incident before core dumps were disabled. Do not describe the entire night as having zero global swap.
