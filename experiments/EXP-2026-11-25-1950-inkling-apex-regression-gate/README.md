# EXP-2026-11-25-1950: Run the Apex gate on the cumulative Inkling build

**Verdict: FAILED as a combined-runtime candidate.** EXP-1946 remains
an isolated Inkling checkpoint. Preserve the operational Apex binary
and its launcher.

## Configuration

Compare the frozen EXP-1880 binary with the cumulative EXP-1946
Inkling binary when both serve Apex. RTX 4070 and the same saved
32768-input, 512-output request. Context remains 262144, batch and
ubatch 4096, Q4_0 K/V, 6290 MiB expert limit, threads 8/16 and the
original CPU masks. The launch environment comes from the frozen
checkpoint. Use one fresh request for each successful diagnostic
arm. Both large-core dumps and competing model servers were absent
during the timed requests.

The strict frozen model initialization first stopped at 5301088
of 5301139 resident pages. A targeted read reduced the missing
count from 51 to zero. The retried pinned reference registered
19504005120 bytes across 120 ranges and completed at 2869.51
prefill and 79.72 decode tok/s. System swap-out grew by 452084
pages during startup and 3680 during the request. The process
reported 38684 KiB in swap. This does not qualify the published
Apex profile. The pinned run was not repeated on the candidate.

For a matched diagnostic, set `GGML_CUDA_REGISTER_HOST=0` in
both launches. The rest of the fixed configuration remains the
same. The pageable pair uses zero pinned expert ranges and the
same 6592856064-byte effective expert cache.

## Matched pageable pair

| Apex 32K/512 | Frozen EXP-1880 | EXP-1946 combined candidate |
| --- | ---: | ---: |
| Prefill, tok/s | 1744.21 | 1673.61 |
| Decode, tok/s | 46.374 | 17.406 |
| Client request, s | 29.902 | 49.251 |
| Peak board VRAM, MiB | 10707 | 10477 |
| Peak process RSS, KiB | 21474144 | 21469884 |
| Expert fills | 72508 | 68104 |
| Expert payload, GB | 138.222 | 130.584 |

All 512 output IDs and response text match exactly. During the
requests both processes have zero swap, with no system swap-out.
The launches had 225 and 423 system swap-out pages before the
requests. The candidate loses 62.5% of decode throughput in this
pair despite similar memory and lower recorded expert traffic.
These timings apply to the matched pageable configuration.
A no-swap performance comparison under the published pinned
profile remains open.

## Integration evidence

The frozen and candidate archives have different `main.o`,
`model-host.o`, `graph-execute.o` and `cuda-driver.o` hashes.
The frozen source uses routing mailbox, predicted-route expert
prefetch and split decode. The observed reference log shows the
mailbox initialized and expert prefetch active. The candidate
inherits an earlier common driver source. Its analyzed driver
source lacks these mechanisms as well as specialized dense MMVQ,
packed Q4 Flash, fixed FWHT, compact route and router softmax
entries from the frozen Apex checkpoint. The recorded source
string inventory contains 30 reference-only option names and
19 reference-only kernel names. This inventory is evidence of
the common-line mismatch; it does not apportion the decode
difference among these features.

For the next combined build, keep the accepted Apex runtime
and driver as base and add Inkling-specific entry points and
images on top. Verify the resulting source and image selection
before the end-to-end numerical and memory gates for both
architectures. Do not promote the current combined binary.

The frozen Apex checksum manifest still passes. No operational
binary, model file or launcher changed. See `inputs.sha256`,
`driver-diff.json` and `results.json`. Raw logs are under
`build/inkling-apex-gate/ab`.
