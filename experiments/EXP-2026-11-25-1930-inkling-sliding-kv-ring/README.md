# EXP-2026-11-25-1930: Measure a mirrored Inkling sliding KV cache

**Verdict: REJECTED.** Keep the EXP-1925 executable and the EXP-1928
higher-memory profile. The mirrored ring eliminates most bulk KV
copies but slows end-to-end decode.

## Mechanism

The original sliding path stages the chronological KV window
in a device workspace and copies the retained rows back each visit.
The candidate allocates a duplicated sliding KV span per visit.
Batched prefill still runs through the existing workspace. On the
first single-row step, it initializes the second copy of the
history. Each following step can view a contiguous chronological
range at its ring offset, append the new key and value, and refresh
only the corresponding mirror row. Positions are mirrored as well.
The oldest key is dropped before the full-window step, matching
the original kernel's cropped view. The Flash image and its
logical key order are unchanged. Full-attention slots are
unaffected. The control uses `I2_SLIDING_KV_RING=0`.

## Timed comparison

RTX 4070. Same 2048/256 prompt and sampling fixture, context
36864, batch 4096, ubatch 2048, Q4_0 K/V, resident wide vectors,
576 MiB activation arena, and 6720 MiB expert budget. One
fresh and one repeat request per launch in A–B–B–A order.
Both arms enabled the full internal fingerprint checks.

| Paired mean | Original | Mirror ring |
| --- | ---: | ---: |
| First prefill, tok/s | 201.71 | 202.07 |
| Repeat prefill, tok/s | 215.89 | 215.24 |
| First decode, tok/s | 14.506 | 14.164 |
| Repeat decode, tok/s | 14.621 | 14.251 |
| Repeat decode, ms | 17441.0 | 17893.9 |
| Repeat client request, s | 27.140 | 27.623 |

Repeat decode regressed 2.53%, with 481 and 424 ms lost in the
two pairs. All 5118 state fingerprints and 256 output IDs and
text per request matched, as did prefill expert traffic. The
board peaks rose 88 MiB within both pairs. The allocated mirror
shape adds about 45.3 MiB of KV payload, with the rest of the
board peak delta subject to allocator and sampling effects.
Peak RSS varied by less than 2 MiB. Process swap and system
swap-out were zero. The fixture uses a full sliding window
and tests fresh process and request reset transitions. It does
not exercise a complete 1024-step traversal in one decode.

## CUDA trace

The candidate used the 2048/64 diagnostic request. Compare to
the earlier EXP-1929 control trace using the same kernel path,
configuration and fixture. This trace comparison is independent
from the timed pairs. It includes initialization of the mirrored
span on the first decode step.

| Per interval | Prior contiguous control | Mirror ring |
| --- | ---: | ---: |
| D2D-exclusive time, ms | 0.393 | 0.101 |
| Bulk 589824-byte copies | 160 | 1.27 |
| Summed Flash Attention kernel time, ms | 9.379 | 11.062 |
| Whole interval, ms | 73.806 | 75.070 |

The copy saving was outweighed by the higher Flash kernel time
in these traces. The trace does not isolate whether changing
device addresses, cache locality or another secondary effect
caused that difference. A future variant should preserve or
measure the memory locality of the Flash input while reducing
the staging cost.

## Reproduction

Apply `source/native-kv-driver.h.patch` with `patch -p1` to the
EXP-1925 isolated driver inputs. Copy the build helper into the
same layout and relink with the EXP-1921 host archive and
numerical object. See `results.json` for the exact arguments.
Use the EXP-1925 decode analyzer for the trace.

The frozen Apex checksum manifest passed. No Apex performance
run or Inkling 32K/512 run was performed for this candidate.
