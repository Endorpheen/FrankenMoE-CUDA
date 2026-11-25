# EXP-2026-11-25-1919: Reject the sixteen-row Inkling Q8 warp tile

**Verdict: REJECTED.** The isolated candidate preserved the numerical
state and increased repeat prefill by 4289.4 ms of elapsed time.

## Hypothesis

Decoding a Q8_0 weight once per 16 activation rows could reduce
weight reads and dequantization relative to the accepted EXP-1913
eight-row tile. A dedicated CUDA entry point keeps the eight-row
and single-token implementation separate. The host selects the
separate entry only for dense Q8 batches divisible by 16. The same
binary dispatches the original kernel with `I2_Q8_TILE16=0`.
Goose keeps one FP32 accumulator per output and the same FMA and
shuffle-reduction sequence.

## Screening pair

Use the EXP-1918 host candidate and the EXP-1917 2048/256 fixture on
RTX 4070. Context 4096, batch 2048, ubatch 1024, Q4_0 K/V. The original
and candidate each handled one first request and one repeat. Both
enabled identical state hashing and full prompt recomputation.

| Metric | Q8 tile 8 | Q8 tile 16 |
| --- | ---: | ---: |
| First prefill, tok/s | 183.89 | 132.17 |
| Repeat prefill, tok/s | 194.00 | 137.95 |
| Repeat prefill, ms | 10556.8 | 14846.2 |
| First decode, tok/s | 13.450 | 13.447 |
| Repeat decode, tok/s | 13.475 | 13.239 |
| Peak board VRAM, MiB | 9963 | 9963 |
| Peak process RSS, KiB | 13331688 | 13331280 |

Both launches match all 5118 state fingerprints, all 256 output
IDs and text for both requests, and the expert traffic. Process
swap and system swap-out were zero. The prefill loss repeated
across both cache states and reached 28.9% in the repeat, so the
screening stopped before the reverse order. Keep the eight-row
baseline and its binary. The frozen Apex checkpoint checksum
manifest passed. No combined-runtime Apex gate ran.

The generated SM89 code uses 64 registers per thread for the
16-row kernel and 40 for the original entry, with no stack frame
or spills in either. The additional accumulator and address state
increases register pressure and lowers theoretical warp residency.
The timing does not isolate the contribution of register pressure,
grid parallelism or instruction scheduling. Do not extrapolate
the loss to an implementation that shares weights across
independent warps while preserving their output arithmetic.

## Reproduction

Apply the three source patches with `patch -p1` to the EXP-1913
Q8 source and matching driver header. Copy the frozen EXP-1916
`cuda-driver.c`, `cuda-driver-reference.c` and its supporting
Inkling headers and GPU images into the sibling build directory.
Keep the EXP-1918 `numeric.o` and host archive as link inputs.
The build helper uses KeelWing, Clang 18, GCC 13 and ptxas for SM89.
See `inputs.sha256`, `ptxas.log` and `results.json`. Disable the
experimental dispatcher with `I2_Q8_TILE16=0` or use the unchanged
EXP-1918 executable.

The next structural opportunity is the activation boundary
identified by EXP-1917 and EXP-1918. Preserve the output in a
device buffer, run the exact post-convolution there, and measure
the full phase and transfers before broadening the resident path.
