# EXP-2026-11-24-1895 — Match GPU-lane IQ4 accumulation on AVX2

**Proven numerical prototype, no placement promotion.** The model-level
diagnostic routes selected physical layers to a separate CPU module.
Goose schedules indexed rows and independent worker ranges. An AVX2/FMA
leaf unpacks IQ4_XS and IQ4_NL and follows the GPU reference's FP32 product,
per-lane fused multiply-add and butterfly order. Intermediate activations
remain F32. No approximate input quantization or external inference
engine is involved.

RTX 4070, Ryzen 9 5950X, Inkling AD-IQ4_XS, prompt 24 IDs, prefill 16+8,
same weight budget, CUDA Attention and resident graph profile. All
recorded token IDs and FP32 state/logit fingerprints match within
each corresponding workload. No system swap-out.

| Workload and placement | Prefill s | Decode tok/s | Expert H2D GB |
| --- | ---: | ---: | ---: |
| 24/17, original GPU control | 3.098 | 5.454 | 8.544 |
| 24/17, one CPU layer / one thread | 3.252 | 5.355 | 7.915 |
| 24/17, worker-capable GPU control | 3.156 | 5.446 | 8.544 |
| 24/17, one CPU layer / four threads | 3.181 | 5.451 | 7.915 |
| 24/17, six CPU layers / four threads | 3.225 | 5.273 | 5.672 |
| 24/17, six CPU layers / eight threads | 3.099 | 5.339 | 5.672 |
| 24/65, GPU control, context 256 | 3.161 | 5.605 | 14.298 |
| 24/65, six CPU layers / eight threads, context 256 | 3.143 | 5.458 | 7.565 |

The independently measured scalar path in EXP-1894 took 129.95 s
across 18 MoE graph calls. The one-thread AVX2 diagnostic takes 0.307 s
for the same 18 calls, and four workers take 0.152 s. The isolated
mechanism is suitable for follow-up on mixed execution. Six fixed
CPU layers save 47.1% of expert uploads at 24/65, but decode falls
2.6%. There is only one paired measurement of each shape. Peak RSS
in the long pair stays near 10.70 GiB. GPU allocation is approximately
25 MiB higher in the candidate (allocation delta, not sampled VRAM peak).
Do not promote fixed CPU placement on these results.

The next investigation prioritizes batched prefill: tiled attention
computing relative score and sink inside the tile, batched K/V and
convolution updates, resident inter-projection buffers, and expert reuse
across prompt rows. Mixed CPU/GPU placement remains experimental
and is independent of Apex EXP-1880.

## Reproduce

Patch the EXP-1894 diagnostic harness with `source/simd-harness.patch`.
Compile `source/simd.goose` with standalone KeelWing and compile generated
C with GCC 13 `-O3 -Dgoose_init=i2_simd_init
-Dgg_init_globals=i2_simd_globals`. Compile the CPU leaf with
`-O3 -mavx2 -mfma -mf16c -ffp-contract=off`. Link the original archive
with `source/hook.c`, the Goose and AVX2 objects, and
`-Wl,--wrap=fg143_graph_regular` alongside the existing cache adapter
wraps. Start with `I2_CPU_MOE_MASK=0`, then `0x10` or `0x3f`
and `I2_CPU_EXPERT_THREADS=4` or `8`. For the longer pair set
context_size=256 in the diagnostic and `I2_TEST_DECODE_STEPS=64`.
Use identical EXP-1892 settings and the input IDs in [results.json](results.json).
