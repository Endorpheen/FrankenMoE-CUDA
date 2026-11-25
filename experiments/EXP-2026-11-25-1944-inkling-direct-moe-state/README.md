# EXP-2026-11-25-1944: Screen direct Inkling MoE state output on 8192/256

**Verdict: INCONCLUSIVE for promotion.** Retain EXP-1943 as the
runtime checkpoint. The 8192/256 fixture is the working test for
subsequent optimization.

## Candidate

EXP-1943 binds both graph results directly to the prefill host
workspace. This candidate also eliminates the copy of the MoE
output from that workspace into scattered token records. The
resident path reuses the otherwise idle legacy attention storage
as a contiguous matrix of current token states. Each token record
references its row. The output binding points to that matrix only
during MoE, after the previous inputs and residuals have been
prepared. Attention still uses its separate readback area. The
fallback layout, loop history, cache and device images are preserved.
Use `I2_DIRECT_MOE_STATE=0` for the same-binary control.

## Goose measurements

RTX 4070 with context 36864, batch and ubatch 4096, Q4_0 K/V,
736 MiB activation arena, wide vectors and the 6720 MiB expert
limit. All runs recompute the prompt. State hashing is enabled
equally. Both fixtures use prefixes of the frozen EXP-1927 input.

The 4096/128 screen used A–B–B–A, each launch with first and
repeat requests. An earlier overlapping disk scan affected the
timing of a separate candidate launch. That launch was excluded
from the speed comparison and retained as a correctness check.

| 4096/128 repeat mean | Control | Direct |
| --- | ---: | ---: |
| Prefill, tok/s | 264.50 | 272.15 |
| Decode, tok/s | 17.955 | 17.519 |

The user switched the working test to 8192/256 during startup
of a 32K/512 candidate run. The full run was canceled before
a result, and the new fixture used A–B–B–A followed by one
additional control. Each fresh process received a first request
and a repeat. The values below are medians across runs.

| 8192/256 | Control | Direct |
| --- | ---: | ---: |
| First prefill, tok/s | 256.18 | 260.59 |
| First decode, tok/s | 18.278 | 18.264 |
| Repeat prefill, tok/s | 259.03 | 262.91 |
| Repeat decode, tok/s | 18.131 | 18.160 |
| Repeat client time, s | 45.911 | 45.417 |

The repeat prefill difference is 1.50%. In the first pair,
repeat decode declined from 18.390 to 18.134 tok/s. In the
reverse order the control fell to 14.713 tok/s on its repeat.
The cause is unknown. The additional control gave 18.131 tok/s.
The five runs are all preserved, and the medians are descriptive.
This screen does not establish a repeatable decode gain.

All 8702 fingerprints per launch and 128 output IDs match
on the short fixture. All 17406 fingerprints per launch and
256 IDs match on 8192/256. The text and expert traffic match in
each fixture and cache state. On 8192/256 the maximum board
peak was 11735 MiB for the control and 11776 MiB for the
candidate. Peak process RSS increased about 16 MiB. There was
zero process swap and system swap-out. The frozen Apex manifest
passed. Apex throughput was not measured.

## llama.cpp reference on 8192/256

One fresh request per profile using the pinned EXP-1927
llama.cpp revision, same prompt, sampler, context, logical batch
and Q4_0 K/V, with ubatch 128 and native auto-fit placement.

| Fit target | Prefill, tok/s | Decode, tok/s | CUDA model, MiB | CUDA compute, MiB |
| ---: | ---: | ---: | ---: | ---: |
| 768 MiB | 86.86 | 12.49 | 1540.15 | 6731.45 |
| 512 MiB | 88.17 | 12.73 | 1596.39 | 7256.68 |

Both responses match all 256 IDs and text in the fresh Goose
control. Both had zero swap and swap-out. These profiles did not
reproduce the 7675.91 MiB GPU model allocation from the stronger
EXP-1927 32K/512 run. Use them as observed launch references.
They do not supersede the stronger upstream decode target.

## Reproduction

Apply `source/prefill.h.patch` to local EXP-1943, then use the
build helper to recompile its Inkling backend and reuse the
EXP-1943 device objects. Generate the compact request by
taking the first 8192 input IDs from EXP-1927 with
`n_predict` set to 256. See `inputs.sha256` and `results.json`
for hashes, complete launch arguments, memory, timing and
correctness records.
