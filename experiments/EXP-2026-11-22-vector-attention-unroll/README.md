# EXP-1698/1699 — Reject explicit Vector Attention row unrolling

These tests preserve the original four-warp geometry and 64
long-context partitions. The two hot 32-key loops of the
EXP-1665 extracted Goose device fragment are expanded in source
order. The supplied generator reproduces both tested sources
byte for byte. No other device fragment, driver setting or
host object is changed.

The existing deterministic Q4 Attention fixture drives both
kernels with identical Q/K/V. At 32,768 keys both variants and
their controls have the same maximum absolute error versus
CPU, 4.61339951e-5. At 4,097 keys the error remains 1.33514404e-5.

| Fixture at 32,768 keys | Registers/thread | Eight long calls ms | Mean call ms |
|---|---:|---:|---:|
| Full-unroll control | 76 | 2.700 | 0.338 |
| Full 32-row unroll | 255 | 7.297 | 0.912 |
| Partial-unroll control | 76 | 2.760 | 0.345 |
| Four-row unroll | 126 | 4.384 | 0.548 |

Neither variant advanced to a full Apex benchmark. The retained
default checkpoint remains unchanged. Along with EXP-1696 and
EXP-1697, these results close this unrolling/geometry sweep.
They do not rule out a fundamentally different attention layout.

Local debug artifacts and traces are preserved in
`build/exp1698-flash-unrolled-rows` and
`build/exp1699-flash-unroll4-fixed`. Each contains generated
source, PTX, ptxas output, full checksums and both fixture traces.
