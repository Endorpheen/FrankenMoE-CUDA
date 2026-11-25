# EXP-2026-11-25-1928: Measure Inkling expert cache capacity at full context

**Verdict: PROVEN for the paired 4096/128 Inkling profile.** The
6720 MiB budget remains a higher-memory experimental profile.
One full 32K/512 replay also completed with bit-exact output.

## Paired capacity probe

Use the EXP-1925 binary with context 36864, batch 4096, ubatch
2048, Q4_0 K/V, 576 MiB activation arena and resident wide vectors.
Change only the expert cache capacity. The default fits 1455
paired expert planes in 6275543040 bytes. The explicit 6720 MiB
limit fits 1633 pairs in 7043272704 bytes. The full context and
all model settings remain identical.

RTX 4070. Four independent server launches in A–B–B–A order
each handled a fresh 4096/128 request and a repeat. Use the first
4096 IDs from the EXP-1927 full fixture, with prompt recomputation
and matched internal hashing. Per-request cache counters were
obtained from `/props` and the prefill phase receipt. Decode
counters are the request deltas with the measured prefill totals
subtracted.

| Mean | Default capacity | 6720 MiB limit |
| --- | ---: | ---: |
| First prefill, tok/s | 201.53 | 201.96 |
| Repeat prefill, tok/s | 205.73 | 206.44 |
| First decode, tok/s | 14.099 | 14.477 |
| Repeat decode, tok/s | 14.065 | 14.424 |
| Repeat client request, s | 29.159 | 28.858 |
| Decode expert fills per request | 4619 | 3465 |
| Decode expert upload, GB per request | 19.922 | 14.945 |
| Peak board VRAM, MiB | 10624 | 11356 |

Repeat decode gained 2.55% on average and improved in both
pairs. Expert fills and payload fell by 25.0%. The peak RSS
difference across arm maxima was 156 KiB. All 8702 state
fingerprints per launch, all 128 output IDs and text for both
requests matched. Process swap and system swap-out deltas were zero.

## Full fresh request

Use the entire frozen 32768/512 request in one new 6720 MiB run.
Compare to the saved EXP-1927 Goose run on the same executable.

| 32K/512 | Default capacity | 6720 MiB limit |
| --- | ---: | ---: |
| Prefill, tok/s | 192.928 | 192.822 |
| Decode, tok/s | 13.615 | 13.917 |
| Decode, s | 37.533 | 36.719 |
| Client request, s | 207.621 | 207.150 |
| Board peak, MiB | 10542 | 11307 |

All 512 IDs and text match the baseline and the fresh upstream
result in EXP-1927. Full prefill fills and bytes were identical
at 40619 and 175.19 GB. The larger cache recorded 11970 decode
fills and 51.63 GB decode expert uploads. The saved full
default run lacks the final `/props` receipt, so its precise decode
transfer count is unknown. Full-configuration time is one
observation per arm. Both had zero process swap and system
swap-out.

The full reference from llama.cpp remains 21.369 tok/s decode.
The larger cache still trails it by 12.81 seconds over 511
intervals. At 18.49 GB/s the larger cache's 51.63 GB represents
2.79 seconds of transfer service at that reference rate. This
estimate excludes host waits and overlap and does not bound the
total software cost of expert misses. The capacity experiment
supports investigating GPU dataflow and submission overhead
alongside cache policy.

The runtime sources and deployment configuration are unchanged.
The frozen Apex checksum manifest passed. EXP-1925 remains the
executable checkpoint. See `inputs.sha256` and `results.json` for
commands, counters and results. The raw runs are under
`build/inkling-exp-baseline/device-attention/inkling-expert-capacity4096`
and `build/inkling-exp-baseline/device-attention/inkling-32k-q8-resident-job`.
