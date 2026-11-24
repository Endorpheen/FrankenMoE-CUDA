# EXP-2026-11-24-1864 — Narrow the dense Q4 pipeline CTA

**REJECTED for the full Apex profile.** The isolated kernel is faster.
The prefill percentage gain is smaller than the decode percentage loss
in both full-model pairs. EXP-1863 remains the research baseline.

The full-window trace records 1680 launches and 916.87 ms in
`goose_mmq_q4_dense_m128n256_pipeline`. The candidate uses the same
prepared Q4 and packed Q8 layouts, warp tile and accumulation order.
A CTA covers 128×128 with 128 threads and 40 KiB shared memory,
compared with 128×256, 256 threads and 60 KiB. Each warp stages
two adjacent prepared-weight tiles instead of one. CUDA reports
one vs two active CTAs per SM. Both builds report zero spills,
with 226 vs 227 registers per thread.

The standalone probe matches the output words of all six shapes,
including partial columns and a nonzero route offset. Median kernel
time decreases by 5.7–20.7% across the five timed shapes.
See [probe.py](probe.py) and [raw GPU-event timings](probe.log).

## Full Apex 32K/512

RTX 4070, context 262144, cache 6290 MiB, batch and ubatch 4096,
Q4_0 KV, one warm-up and one scored request per fresh server.
A-B-B-A on the same executable. A sets `GOOSE_MMQ_Q4_NARROW=0`,
B leaves the variable unset. EXP-1863 causal Flash remains active in both.

| Pair | Prefill control → candidate | Prefill change | Decode control → candidate | Decode change |
| --- | ---: | ---: | ---: | ---: |
| A1 → B1 | 3347.57 → 3397.92 | +1.50% | 83.28 → 81.84 | −1.74% |
| B2 → A2 | 3357.54 → 3398.44 | +1.22% | 83.66 → 81.67 | −2.38% |
| Mean | 3352.55 → 3398.18 | +1.36% | 83.47 → 81.75 | −2.06% |

Mean prefill saved 131.27 ms and decode took 128.79 ms longer.
Mean client time was 15.98518 vs 15.98733 seconds. All eight replies
matched the 512 IDs, text and SLRU counters. Peak sampled VRAM was
8774 MiB in all arms. Peak RSS ranged from 21490836 to 21493336 KiB.
Process swap and system swap-out were zero. The percentage loss exceeds
the gain by 0.70 percentage points on the means. These observations
establish a decode regression for the full profile. They do not
attribute it to a direct change in the decode kernel, which was unchanged.

## Reproduction

Use EXP-1786's frozen MMQ device tree and Clang 18, KeelWing and CUDA
12.6 as in [EXP-1859](../EXP-2026-11-24-1859-flash-interior/README.md).
The unmodified MMQ Goose source emits mmq-device.h SHA-256
`9edab6645ad65dfcb6bed8bc23e3ba25f5caaa5bacb03570958db11fae8b2191`.
Apply the two device patches under [source](source), emit the `mmq`
fragment from Goose and build the MMQ image with the same compiler flags.
Target `goose_mmq_q4_dense_m128n256_pipeline` with ptxas for both arms,
taking the control PTX directly from the frozen image.
Name the cubins `control.cubin` and `narrow.cubin` beside `probe.py`.

For the full runtime, apply the driver patch to EXP-1863, embed
`narrow.cubin` as `static const unsigned char mmq_q4_narrow_image[]`,
compile only cuda-driver.o and relink the EXP-1863 archive as in that
experiment. The local rejected checkpoint contains the binary and
`argv.txt` and `env.txt`. The full run identifiers and raw timings are
in [results.json](results.json). Preserve the old full-model baseline.
If a future candidate changes the decode interaction, remeasure the
combined path before using the narrow CTA in the complete profile.
