# EXP-2026-11-25-2047 — Exact warp layouts for Bonsai verification PQ2

**Verified decode improvement for four verifier rows.** Compare one,
two or four output rows per warp and byte-permutation or bit-spread
PQ2 unpack. The signed Q8 packet, integer products, per-group scale,
accumulation and butterfly order are unchanged. The four-row candidate
uses four output rows for trunk matrices and two for vocabulary, with
byte permutation. The five-row candidate uses four and bit spreading.

The CUDA fixture checks all 256 packed code bytes and 3,373,044 real
projection outputs on twelve fixtures. Rotating four distinct gate,
down and QKV matrices exceeds L2. Vocabulary is a single 338 MB
matrix. For four inputs, the vocabulary kernel improves 1.275 to
0.750 ms. For five, it improves 1.616 to 0.750 ms, and the rotating
gate/down/QKV kernels improve approximately 10–16%. See component data
for cache-sensitive and alternative shape measurements.

## RTX 4070, 32K/512

A–B–C–C–B–A, one invocation at a time, same host and CUDA image,
Bonsai PQ2_0-mtp, Q4_0 K/V, context 36,864 and ubatch 1024.
A uses the existing four-row layout, B the tuned four-row layout,
C the tuned five-row layout. Decode counts 511 intervals.

| Arm | Prefill, tok/s | Decode, tok/s | Target, ms/block | Combined, s |
| --- | ---: | ---: | ---: | ---: |
| A1 | 623.00 | 70.81 | 41.480 | 59.813 |
| B1 | 618.82 | 72.16 | 40.617 | 60.034 |
| C1 | 618.08 | 70.90 | 47.797 | 60.223 |
| C2 | 616.36 | 70.88 | 47.841 | 60.373 |
| B2 | 617.39 | 72.08 | 40.569 | 60.165 |
| A2 | 616.25 | 70.50 | 41.608 | 60.422 |

Mean four-row decode rises **70.65 → 72.12 tok/s (+2.08%)**,
with **0.951 ms** saved per target call. Mean prefill is
619.63 → 618.10 tok/s; the combined pairs disagree
(mean 60.118 → 60.099 s). Five-row mean is 70.89 tok/s and
still trails tuned four-row with 150.58 MiB more device allocation.
Keep four rows. A and B have identical device allocation sizes.

All six outputs match the EXP-2031 oracle, including all 512 IDs
and selected logits. Both 255-token probes compare all retained
prefix states, forced rejections and the subsequent decode step.
A and B accept 365/435; C accepts 386/484. Process swaps and major
faults are zero. Peak VRAM is not sampled. No Apex, Inkling or
production source changes.

## Reproduction

Run from the root with EXP-2025, EXP-2032, EXP-2038 and EXP-2043–2046
build trees present.

```bash
exp=experiments/EXP-2026-11-25-2047-bonsai-mtp-pq2-warp-rows
out=build/bonsai-mtp-warp-rows-repro
bash "$exp/source/build-image.sh" "$out"
bash "$exp/source/build-host.sh" "$out"
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" build/bonsai-integration build/bonsai-integration/inventory.json "$out/mvq.cubin"
```

Use the EXP-2046 long profile, with `"$out/bonsai-mtp-warp-rows"`
and `"$out/mvq.cubin"`. Set `BONSAI_MTP_VERIFY_ROWS=4` in A,
and add `BONSAI_MTP_MVQ_SHAPE=1` in B. In C use
`BONSAI_MTP_VERIFY_ROWS=5 BONSAI_MTP_MVQ_SHAPE=1`.
Repeat sequentially in the order shown above. The short checks use
the same candidate flags plus `BONSAI_MTP_TARGET_PROBE=1` with
the EXP-2046 255-token probe.

[Measured samples and artifact hashes](data/)
