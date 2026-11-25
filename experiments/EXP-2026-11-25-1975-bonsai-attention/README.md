# EXP-2026-11-25-1975 — Bonsai full-attention layer on CUDA

**PROVEN for component correctness.** This fixture validates the full-attention
layer at block 3 on real Bonsai weights.

Hypothesis: the accepted Goose RoPE, Q4_0 row-write, and tiled attention kernels
support the 24 query heads, four KV heads, and 256-channel heads without
changes. The model-specific projection path splits interleaved Q and gate by
head and applies sigmoid gating before the Hadamard-folded output projection.

The layer executes PQ2 embedding lookup, inverse embedding transform, input
RMS norm, Q/G/K/V projections, per-head RMS norm, text IMRoPE (64 dimensions,
sections 11/11/10/0), causal Q4_0 KV, attention, output projection, and the
dense SwiGLU FFN with residuals. The diagnostic keeps the cache on the GPU
between tokens. NumPy independently executes the layer and packs Q4_0 rows.

| RTX 4070 fixture | Result |
| --- | --- |
| Token IDs | 1511, 17, 248046, 1511 |
| Positions | 4093 through 4096 |
| Live KV rows | One through four |
| Max absolute activation error | 4.145e-5 |
| Layer-output comparison | Pass at atol 2e-4 and rtol 5e-4 |
| KV row bytes | Exact parity on all four positions |
| Cache clear and replay | Exact GPU output and row bytes |

The position numbers exercise RoPE at the 4096 boundary. The fixture does not
simulate a 4096-row history or measure runtime latency, memory peaks, generation,
or full-model throughput. Shared CUDA source and the frozen image are unchanged.
The next integration work is a device-resident trunk with both layer kinds,
followed by final normalization, logits and normal runtime entry points.

## Reproduction

Run from the repository with the EXP-1973 build and frozen Apex device header.
Clang 18 and ptxas assemble the supplemental kernels for sm_89. NumPy is
diagnostic-only.

```sh
exp=experiments/EXP-2026-11-25-1975-bonsai-attention
bash "$exp/source/build.sh" build/bonsai-attention-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-attention.py" \
  build/bonsai-integration build/bonsai-integration/inventory.json \
  build/bonsai-attention-repro/attention.cubin \
  build/checkpoint-apex-exp1880-before-inkling/build-inputs/goose-cuda-image.h
```

[Results and image hashes](results.json).
[Prism Qwen3.5 full-attention graph](https://github.com/PrismML-Eng/llama.cpp/blob/prism/src/models/qwen35.cpp).
