# EXP-2026-11-25-1949: Screen reuse of Inkling loop-state RMS

**Verdict: INCONCLUSIVE for throughput.** Keep EXP-1946 as the
8192/256 baseline. This single pair does not support promotion.

Inkling reuses a saved loop residual to construct messages for
multiple layers after a return to physical layer 6. With the
normalized history path enabled, the original computes RMS of the
same residual on each message calculation. The candidate stores
the normalized residual when capturing that loop state, then
calls the same depth mixing routine with the saved normalization.
It applies in prefill and decode and uses the existing storage.
The fallback remains for raw history or `I2_CACHE_LOOP_RMS=0`.
The arithmetic, attention, weights, scheduling and device objects
are otherwise unchanged.

## Screen

RTX 4070, same binary, same EXP-1944 8192/256 fixture. Context
36864, batch and ubatch 4096, Q4_0 K/V, 736 MiB arena and 6720 MiB
expert limit. One launch per arm, first and repeat requests with
full prompt recomputation and identical state fingerprinting.

| Metric | Original | Cached loop RMS |
| --- | ---: | ---: |
| First prefill, tok/s | 276.93 | 277.06 |
| Repeat prefill, tok/s | 282.16 | 279.80 |
| First decode, tok/s | 18.380 | 18.488 |
| Repeat decode, tok/s | 18.349 | 17.933 |
| Repeat client, s | 43.151 | 43.721 |

First-request timing was nearly flat. On the repeat, prefill
fell 0.84% and decode fell 2.27%. The result does not isolate the
cause of the observed slowdown. No reverse pair was run.
All 17406 fingerprints per launch, all 256 IDs and response
text, and expert traffic match. Peak board memory is 11711 and
11712 MiB, and peak process RSS differs by less than 1 MiB.
There was zero process swap and system swap-out.

The frozen Apex manifest passed. No Apex performance check or
32K/512 experiment was performed. The production runtime and
launch profiles were unchanged.

## Reproduction

Apply the two host patches to EXP-1943 and use the build helper
to link the frozen EXP-1946 archive. Use `inputs.sha256` and
`results.json` for the build identities, exact launch flags,
timing and correctness measurements.
