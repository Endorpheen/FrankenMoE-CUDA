# EXP-2026-11-25-1931: Test warp-parallel Inkling decode softmax

**Verdict: REJECTED for the 32K/512 target.** The short paired
speedup did not carry to the full workload. Keep EXP-1925 with
the EXP-1928 6720 MiB profile.

## Mechanism

The existing single-query kernel assigns the 32 relative score
adjustments per key tile to one warp, then one lane computes
the maximum, exponentials and ordered denominator. The
candidate uses an additional single-query entry. It computes
the max with warp shuffles, evaluates each independent
exponential in its own lane and converts to FP16 with the same
instruction. The leader sums the probabilities in exactly the
original order. Query and value MMA, partial layout, partition
boundaries and masks remain the same. Prefill uses the existing
kernel. The control uses `I2_DECODE_WARP_SOFTMAX=0`.

## Paired 2048/256 screen

RTX 4070, context 36864, batch 4096, ubatch 2048, Q4_0 K/V,
resident wide vectors, 576 MiB activation arena and an explicit
6720 MiB expert budget. Four launches in A–B–B–A order, with
one first and repeat request per launch, full prefill and matched
state hashing.

| Paired mean | Control | Warp softmax |
| --- | ---: | ---: |
| First prefill, tok/s | 201.55 | 202.51 |
| Repeat prefill, tok/s | 215.27 | 215.48 |
| First decode, tok/s | 14.537 | 14.762 |
| Repeat decode, tok/s | 14.576 | 14.775 |
| Repeat decode, ms | 17494.4 | 17259.0 |
| Repeat client request, s | 27.227 | 26.974 |

All 5118 fingerprints per launch, both sets of 256 IDs and text
and prefill expert traffic matched. Repeat decode saved 239 and
232 ms in the two pairs. Board maxima were 11344 and 11360 MiB
across the control and candidate arms. Peak RSS was within 1 MiB
and process swap and system swap-out were zero.

## Full target workload

One fresh 32K/512 candidate request compared to the recorded
EXP-1928 control at the same settings, without internal
fingerprinting in either timed full test.

| 32K/512 | EXP-1928 control | Warp softmax |
| --- | ---: | ---: |
| Prefill, tok/s | 192.82 | 192.71 |
| Decode, tok/s | 13.917 | 12.919 |
| Decode, s | 36.719 | 39.555 |
| Full client request, s | 207.150 | 210.052 |
| Peak board VRAM, MiB | 11307 | 11289 |

All 512 IDs and the text match. Prefill expert fills and payload
matched at 40619 and 175.19 GB. Process swap and system swap-out
were zero. The full comparison has one observation per arm, with
a 7.17% observed decode regression and 2.84 seconds added.

The new entry compiled with 232 registers per thread versus 72
for the existing Flash entry, with no spills in either. The
higher register requirement can limit active blocks. Its actual
contribution to the long-request slowdown is unmeasured. A
follow-up should shorten the softmax live ranges and measure
kernel and request time at both short and long key counts
before enabling this path.

## Reproduction

Apply the Flash patch to EXP-1914 and the header patch to
EXP-1925 in the isolated sibling build layout. Run the provided
build helper with KeelWing, Clang 18, GCC 13 and SM89 ptxas.
See `inputs.sha256`, `ptxas.log` and `results.json` for the
build and timing records. The frozen Apex checkpoint manifest
passed. Neither the production profile nor the Apex binary
was changed.
