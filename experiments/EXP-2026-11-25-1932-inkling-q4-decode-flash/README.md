# EXP-2026-11-25-1932: Specialize Inkling single-query Flash Attention for Q4_0

**Verdict: ACCEPTED as the isolated Inkling candidate.** One full-length
same-binary pair gained 9.08% decode throughput. Repeat full-length
performance verification remains open.

## Register investigation

EXP-1931 used 232 registers per thread for the warp-parallel softmax
entry, compared with 72 for the existing mixed-query kernel. Disabling
only the unrolling of the 32-value ordered sum produced 64 registers
without spills. On the full request it returned from 12.919 to 13.941
tok/s, close to the earlier 13.917 tok/s reference. This tests the
unroll change. It does not independently isolate occupancy from
instruction scheduling.

Next, compile the same warp score and softmax flow with the single
query and Q4_0 shape known at compile time. Keep the two ordered
value MMA chunks looped. This combination uses 56 registers and
no spills on SM89. The dispatcher requires a single query, Q4_0 KV
and the existing parallel-scores gate. The baseline kernel handles
other shapes and types. The ordered probability sum, score products,
MMA instructions and their order, masks and partitions are preserved.

On RTX 4070, the CUDA-event probe used seeded Q4_0 data and exact
byte comparisons of all partial outputs and metadata. The timing
alternated forward and reverse variant order. See `flash-bench.json`
for each of the four 80-launch samples, all tested key counts and
output digests. These timings are for the Flash partial kernel only.

| Keys | Existing Flash, us | Warp 232 registers, us | Sum only 64 registers, us | Q4 shape + value loop, us |
| ---: | ---: | ---: | ---: | ---: |
| 1024 | 179.69 | 159.48 | 190.21 | 100.22 |
| 4096 | 306.52 | 271.01 | 328.74 | 182.03 |
| 32768 | 796.08 | 1440.67 | 830.60 | 530.11 |

All seven variants produced identical partial outputs and metadata
at all five key counts. The specialization saves 33.4% of kernel
time in the 32K probe. This comparison excludes the merge, transfers,
and graphs around Flash.

## Full 32K/512 same-binary comparison

RTX 4070, EXP-1927 request and EXP-1928 cache profile. Context
36864, batch 4096, ubatch 2048, Q4_0 K/V, wide vectors, 576 MiB
activation arena and 6720 MiB expert cache limit. One fresh
request per arm. The control uses `I2_DECODE_WARP_SOFTMAX=0`.

| Full request | Control | Q4 specialization |
| --- | ---: | ---: |
| Prefill, tok/s | 192.637 | 192.792 |
| Decode, tok/s | 14.013 | 15.285 |
| Decode, s | 36.467 | 33.432 |
| End-to-end client, s | 207.041 | 203.893 |
| Peak board VRAM, MiB | 11297 | 11275 |
| Peak process RSS, KiB | 13841440 | 13840624 |

The candidate saved 3.035 seconds of decode and matched all
512 token IDs and text. Prefill expert fills and payload were
40619 and 175.19 GB in every full run in this experiment. Process
swap and system swap-out were zero. One additional 2048/256
correctness request matched all 2559 internal fingerprints, IDs
and text of the EXP-1931 control.

The stronger EXP-1927 upstream decode remains 21.369 tok/s.
The remaining difference is 9.52 seconds over 511 decode
intervals. The 25 tok/s and 500 tok/s project targets remain open.
The frozen Apex manifest passed. A combined-runtime Apex performance
gate remains open and production launch settings were not changed.

## Reproduction

Apply both patches to the EXP-1931 sibling build inputs and run
`source/build-candidate.sh` with the same KeelWing, Clang 18,
ptxas and GCC 13 setup. The compiler logs in `compiler` cover
the variant screen and the selected production-shaped kernel.
Use `source/prepare-variants.py` to reproduce the source variants
from EXP-1931. Compile each variant with the same OpenCL driver and
SM89 ptxas command as the build helper. Run `source/bench-flash.py`
from the isolated build directory. The exact model-run arguments
and binary hashes are in `results.json` and `inputs.sha256`.
