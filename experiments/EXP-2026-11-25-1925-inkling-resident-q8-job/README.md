# EXP-2026-11-25-1925: Reuse the zero-offset Inkling Q8 descriptor

**Verdict: ACCEPTED for the isolated Inkling checkpoint.**

Dense single-row Q8_0 operations in Inkling frequently describe
one input and output at offset zero. The CUDA driver now reserves
and zeroes one device descriptor on first use. Eligible operations
pass that device address to the same Q8 kernel. Two-word format,
exactly one job, zero offsets and the Inkling specialized flag are
required. Other matrix calls retain their existing descriptor
uploads. Set `I2_RESIDENT_Q8_JOB=0` to run the original upload on
the same binary. The patch reuses all existing device images,
the EXP-1921 host archive and the EXP-1923 launch profile.

## Timed A/B

RTX 4070, EXP-1917 2048/256 fixture, context 4096, batch and
ubatch 2048, Q4_0 K/V, 576 MiB activation arena. Four launches
in A–B–B–A order recomputed the full prompt for the first and
repeat requests. State hashing was enabled in both arms.

| Paired mean | Control | Resident descriptor |
| --- | ---: | ---: |
| First prefill, tok/s | 202.70 | 201.70 |
| Repeat prefill, tok/s | 214.22 | 214.06 |
| First decode, tok/s | 13.805 | 14.065 |
| Repeat decode, tok/s | 13.766 | 14.036 |
| Repeat decode, ms | 18524.3 | 18167.5 |
| Repeat client request, s | 28.300 | 27.949 |

Repeat decode gained 1.96%. The pairs saved 271.5 and 441.9 ms of
decode. Repeat prefill changed by -0.07% on average. All 5118
fingerprints per launch, output IDs, text and expert traffic
matched. Board VRAM was 10327 MiB for both arms in the forward
pair and 10232 MiB for both in the reverse. Peak RSS stayed
within 1244 KiB. Process swap and system swap-out were zero.

## Decode timeline

The independent profiling requests used the same 2048 input IDs
and a 64-token output budget. Compute the warm window between
the first and last vocabulary readback. The diagnostic control
also times disjoint backend scopes on the host. Both absolute
timings include profiler effects. Their phase values do not
combine with disjoint CUDA intervals.

| Control wall scope | ms per decode interval |
| --- | ---: |
| MoE graph and output | 42.18 |
| Attention graph | 31.55 |
| Vocabulary projection | 2.29 |
| Other backend | 1.80 |
| CPU bridge | 1.04 |

The trace measures 79.59 ms per repeat interval, of which 33.41 ms
show no recorded work on the GPU. During these intervals, 21.82 ms
show no CUDA API activity, 6.97 ms lie inside copy APIs, 3.67 ms
inside launch APIs and 0.87 ms inside explicit sync APIs. These
categories do not identify all causes of absent device work.
The largest summed kernel families are IQ4 expert matrices
(11.85 ms), Flash Attention (9.40 ms) and dense Q8 (7.19 ms).

The candidate trace reduced 16-byte H2D transactions from 689 to
48 per token. Their summed device copy time dropped from
0.237 to 0.013 ms. Copy API wall time with no GPU activity dropped
from 6.97 to 6.21 ms. The small descriptor's cost was primarily
submission overhead. Expert fitting and launch dependencies are
unchanged. The remaining 32-byte route ID reads, expert transfers
and repeated work outside CUDA API scopes merit direct measurement.

## Reproduction

Apply both CUDA driver patches in `source` with `patch -p1` to the
isolated EXP-1916 CUDA driver inputs. Relink with the EXP-1921
host archive and numerical object using the build helper. Follow
`results.json` for the arguments. To reproduce the diagnostic
control, apply the host diagnostic script to the EXP-1921 host
input and run with `I2_DECODE_BUDGET=1`. Use Nsight Systems in
CUDA trace mode, export SQLite and call `analyze-decode.py` with
the profile directory containing `inkling.sqlite`, `server.log`
and `warm.json`.

The frozen Apex manifest passed. This experiment does not run
Apex or check the candidate at 32K/512. It leaves deployment
profiles untouched.
