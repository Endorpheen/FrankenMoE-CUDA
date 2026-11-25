# EXP-2026-11-25-1937: Fuse the Inkling SiTU expert activation

**Verdict: ACCEPTED for the isolated Inkling checkpoint.** Both short
pairs show a prefill gain. One full test preserves correctness and
shows an end-to-end gain on the target workload.

## Mechanism

The original Inkling MoE graph materializes separate gate and up
transforms between its two IQ4 projections. The combined CUDA node
reads the existing gate/up layout and directly writes the down
projection input. It replaces nine scale, tanh, sigmoid and
multiply nodes. The Float32 product and scale order, tanh and
sigmoid formulas and noncontracting arithmetic match the existing
CUDA path. The node uses the same activation buffer planner and does
not alter router decisions, cache policy or expert weight layouts.
It runs for both batched prefill and decode in the resident
Inkling graph. The existing fallback is selected with
`I2_FUSED_MOE_SITU=0`. ptxas reports 16 registers and no spills
on SM89.

## Paired 2048/256 screen

RTX 4070, context 36864, batch 4096, ubatch 2048, Q4_0 K/V,
wide vectors, a 576 MiB activation arena and 6720 MiB expert
budget. Four launches in B–A–A–B order, with one first and repeat
request per launch. Both arms used the same binary, full prompt
recomputation and identical state hashing.

| Paired mean | Control | Fused SiTU |
| --- | ---: | ---: |
| First prefill, tok/s | 203.39 | 206.63 |
| Repeat prefill, tok/s | 216.79 | 221.33 |
| First decode, tok/s | 17.818 | 17.980 |
| Repeat decode, tok/s | 17.753 | 17.871 |
| Repeat prefill, ms | 9446.8 | 9253.2 |
| Repeat client request, s | 24.026 | 23.743 |

Repeat prefill gained 2.09% and both pairs reduced repeat client
time. Repeat decode changed by -0.15% and +1.48% in the two pairs.
The full mean gain of 0.66% in that metric does not establish an
independent decode improvement. All 5118 state fingerprints per
launch, all 256 output IDs and text per request, and prefill
expert traffic matched. Board peak was 11271 MiB across all arms.
Peak RSS stayed within 2 MiB, with zero process swap and system
swap-out.

## Full workload

One fresh 32K/512 candidate request compared to the saved
corresponding EXP-1936 run at the same options.

| Full request | EXP-1936 | Fused SiTU |
| --- | ---: | ---: |
| Prefill, tok/s | 193.225 | 197.141 |
| Decode, tok/s | 17.582 | 17.897 |
| Prefill, s | 169.584 | 166.216 |
| Decode, s | 29.064 | 28.553 |
| Client request, s | 199.113 | 195.230 |
| Peak board VRAM, MiB | 11283 | 11280 |
| Peak process RSS, KiB | 13840592 | 13842912 |

Prefill saved 3.368 s and total request time fell by 3.883 s.
All 512 IDs and text match, as do 40619 prefill expert fills
and 175.19 GB of prefill uploads. Swap and swap-out were zero.
The full comparison has one observation per binary. The stronger
EXP-1927 llama.cpp reference still leads by 4.64 s in decode.

The frozen Apex checksum manifest passed. The new node is scoped
to Inkling and the operational Apex binary was preserved. No
combined-runtime Apex performance or deployment change was made.

## Reproduction

Apply the host patch to EXP-1934, graph dispatcher patch to the
local Inkling Attention graph source, and device wrapper patch
to the local CUDA tanh inputs. Copy `situ.cl` and `situ.h` and
use `source/build-candidate.sh` to compile the isolated SM89
image and relink the EXP-1936 archive. See `compiler.ptxas`,
`inputs.sha256` and `results.json` for settings, build
identities, and the individual timing and correctness records.
