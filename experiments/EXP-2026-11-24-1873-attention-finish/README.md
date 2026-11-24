# EXP-2026-11-24-1873 — Share the barriers across decode Attention heads

**REJECTED in the isolated GPU graph.** The exact EXP-1765 cubin
in EXP-1863 finalizes each of the four heads separately. It reuses
one shared scratch area and synchronizes the CTA three times per
head. This candidate gives each head a disjoint 4608-byte slice.
It stages all maxima, synchronizes, scales and stages all partial
values and totals, synchronizes a second time, and writes all
outputs. It preserves arithmetic order within every head, Q4_0
packing, the existing K/V loop and the merge kernel. Two barriers
replace twelve in the finalization path.

| Cubin | Registers/thread | Shared per CTA | Static spills | Active CTAs/SM |
| --- | ---: | ---: | ---: | ---: |
| EXP-1863 control | 142 | 4608 B | 0 B | 3 |
| Fused finish | 130 | 18432 B | 0 B | 3 |
| Fused finish capped at 128 | 128 | 18432 B | 0 B | 4 |

RTX 4070, ten distinct synthetic KV layers, the same query pack and
output merge as [EXP-1872](../EXP-2026-11-24-1872-attention-regcap/README.md).
The probe alternates A-B-B-A with eight warm-ups and 96 measured
replays per block. Final outputs matched all 40960 FP32 words.
The last layer's partials and metadata also matched byte-for-byte
at all five KV lengths between 17 and 65537.

At KV 33279 / 64 partitions, the mean ten-layer graph took
1711.96 → 1968.01 µs (+14.96%) with 130 registers and
1715.07 → 1933.73 µs (+12.75%) with 128. Both candidates were
slower at every tested shape. Changes in compiled scheduling and
shared residency accompany the removed barriers. Their individual
contributions were not isolated. No full model run or model
memory measurement was made.

Reproduce by restoring the frozen EXP-1786 MMQ device tree with
EXP-1765 manual2's CUDA glue and headers. Apply the two
[source patches](source), then use the Clang 18 SM80 PTX command
from EXP-1859 and CUDA 12.6 ptxas with
`-arch=sm_89 --entry goose_flash_vec_q4_packed_quad`.
For the capped cubin add `--maxrregcount=128`. Copy the accepted
EXP-1765 cubin as `control.cubin`. Run the unchanged EXP-1872
probe with `fused.cubin` and `fused128.cubin` as candidates.
Verify [SHA256SUMS](SHA256SUMS). Raw data appear in
[fused.log](fused.log), [fused128.log](fused128.log) and
[results.json](results.json). Continue from EXP-1863.
