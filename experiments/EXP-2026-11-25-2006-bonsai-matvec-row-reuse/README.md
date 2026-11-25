# EXP-2026-11-25-2006 — Reuse Bonsai decode activations across output rows

**Accepted in the Bonsai experimental HTTP line.** Two outputs assigned
to one warp reuse the same four F32 lane inputs per 128-weight block.
Each output row retains its independent weight scale, four ordered FMAs,
and original warp butterfly. Its K traversal and FP32 arithmetic order
are preserved. The candidate leaves packed weights and memory allocation
unchanged. It selects two rows per warp for single-token projections
with more than 1024 output rows, with the prior kernel on narrower
projections. The GQA change from EXP-2005 stays enabled in both arms.

## Profile of the EXP-2005 control

One separate CUDA-only diagnostic HTTP request, 32768/16, returned the
same first 16 output IDs as PrismML. From the first vocabulary projection
to the end, decode GPU span is 928.976 ms for 15 continuation evaluations:

| Group | CUDA time, ms |
| --- | ---: |
| Ordered PQ2 matvec, 6015 calls | 622.902 |
| Q4 grouped attention and original merge | 120.547 |
| Normalization | 104.527 |
| Decay | 31.842 |
| FWHT | 10.956 |

The largest PQ2 grids are 4352 and 1280 CTAs at 252.939 and 225.370 ms,
respectively. GPU prefill span is 53.531 s, including 31.981 s in PQ2 MMQ,
12.566 s in Flash tiles and 2.796 s in Flash merge. See the
[profile summary](data/profile-exp2005.json). This diagnostic has no CPU
sampling and its times are outside the performance A/B.

## Component checks

Two-row and four-row variants match the exact F32 bytes of the ordered
control on eight actual PQ2 matrices and one shortened QKV row count.
CUDA event medians include repeat launches on one matrix per case.

| Projection | Ordered, us | Two rows, us | Four rows, us |
| --- | ---: | ---: | ---: |
| GDN QKV | 69.72 | 56.22 | 60.40 |
| GDN output | 43.97 | 37.63 | 40.11 |
| FFN gate | 116.63 | 93.19 | 92.53 |
| FFN up | 115.83 | 92.55 | 91.99 |
| FFN down | 114.47 | 97.67 | 102.10 |
| Attention query | 77.22 | 62.21 | 62.92 |
| Attention key | 9.14 | 9.08 | 19.88 |
| Vocabulary | 1767.52 | 1561.67 | 2115.50 |

The two-row and four-row kernels use 37 and 40 registers with no
spills. The broad two-row variant is selected. Four rows improve only
gate/up by less than 1% versus two rows in this probe.

## RTX 4070 HTTP results

Same binary and image in both arms. A sets `BONSAI_MATVEC=ordered`,
B uses the default. Context 36864, batch 4096, ubatch 1024, Q4_0 K/V,
greedy and EOG masked, MTP off, and fixed CPU affinity mask 0f0f.
Both short pairs (A–B–B–A) match all 64 IDs and the full text.
The long pair matches all 512 IDs and the text SHA-256 from EXP-1977.

| Workload | Prefill A → B, tok/s | Decode A → B, tok/s | Decode |
| --- | ---: | ---: | ---: |
| 4097/64, two pairs, aggregate time | 815.83 → 815.87 | 17.441 → 17.939 | +2.85% |
| 32768/512, one pair | 613.54 → 612.45 | 16.198 → 16.651 | +2.79% |

On the long pair, candidate decode is 30.689 s and saves 856.834 ms.
Prefill loses 95.206 ms (-0.18% throughput) on that pair, while short
mean prefill stays within 0.01%. The full long request still saves
roughly 0.76 s. Long peak VRAM is 8932 MiB for both arms. VmHWM is
7,457,092 → 7,457,024 KiB. All requests end with zero process swap and
no system swap-out growth. The existing Apex and Inkling artifacts
were not changed. Full shared-binary regression gates remain open.

The isolated matrix savings do not extrapolate to the full decode.
The retained PrismML CUDA code dispatches PQ2 through a Q8_1 integer
dot path, while this candidate remains FP32. Separate any future
integer-activation experiment from this exact reference and measure
its preparation cost. Such a candidate needs same-history logits and
token-selection checks, longer generations and independently scored
difficult prompts before it can be accepted.

## Reproduction

```bash
ex=experiments/EXP-2026-11-25-2006-bonsai-matvec-row-reuse
base=build/bonsai-decode-gqa-2005-default
out=build/bonsai-row-reuse-repro
bash "$ex/source/build-runtime.sh" "$out"
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/check.py" build/bonsai-integration \
    build/bonsai-integration/inventory.json "$base/combined.cubin" "$out/rows.cubin"
```

Use the EXP-2005 HTTP launcher with `"$out/frankenmoe-infer"` and
`BONSAI_CUDA_IMAGE="$out/rows.cubin"`. Keep its frozen PTX and Flash
flags. Post the EXP-1977 request to `/completion`; for the short
version take 4097 prompt IDs and request 64 output IDs. Set
`BONSAI_MATVEC=ordered` only on the control. See the
[request timings and IDs](data/results.json), the [component timings](data/samples.json)
and the [artifact manifest](data/SHA256SUMS).
