# EXP-2026-11-25-1952: Repeat the Inkling driver comparison with cached weights

**Verdict: large prior regression not reproduced.** Keep both production
checkpoints. The EXP-1951 CUDA object can proceed to a single-executable
integration probe. This block does not establish final non-regression.

## Setup

One request per fresh server, A–B–B–A. A is the accepted isolated EXP-1946
tile32 binary. B is the EXP-1951 Inkling link with its final layout guard.
Use a shared RTX 4070 in sequential runs, initial board usage 1999–2000 MiB.
Use the same 8192-input, 256-output token request and explicit environment,
context 36864, batch/ubatch 4096, Q4_0 K/V and expert limit 6720 MiB.
The prompt is recomputed in every arm. The weight file stays in the OS page
cache. Actual process storage reads and major faults were zero during every
request. No file-cache reset or change to the model was performed.

| Arm | Prefill, tok/s | Decode, tok/s | Request, s | Board peak, MiB | RSS peak, KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A1 | 276.325 | 18.386 | 43.926 | 11795 | 14111556 |
| B1 | 277.110 | 18.215 | 43.975 | 11795 | 14111704 |
| B2 | 276.854 | 18.371 | 43.897 | 11795 | 14111960 |
| A2 | 277.957 | 18.269 | 43.847 | 11795 | 14112324 |
| A mean | 277.141 | 18.328 | 43.887 | 11795 | 14111940 |
| B mean | 276.982 | 18.293 | 43.936 | 11795 | 14111832 |

The mean changes were -0.06% prefill, -0.19% decode and +0.11% client
time. Decode changed in opposite directions between the two pairs.
All 256 output IDs, response text, 8703 state fingerprints and final
`/props` match. Process swap and system request-interval swap-out were
zero in all arms. System startup swap-out was 285–453 pages.

The earlier candidate's 66.175-second response and 1.44 GiB higher peak
RSS did not recur. No storage I/O counters were captured during that
historical request, so its cause remains unverified. A/B does not validate
the Apex pinned profile or the combined host and Goose dispatcher.

## Reproduction

Use the EXP-1946 binary and the final Inkling probe built by
[EXP-1951](../EXP-2026-11-25-1951-apex-driver-rebase-probe/README.md).
Verify binary and fixture hashes in `results.json`. Start each server
with its binary followed by `launch_argv_after_binary` and the recorded
`explicit_env`. After `/health` is ready, issue one request:

```bash
curl -fsS --max-time 120 -w '%{time_total} %{time_starttransfer}\n'     -H 'Content-Type: application/json'     -d @build/inkling-exp-baseline/device-attention/inkling-moe-tile32/request-8192-256.json     http://127.0.0.1:9101/completion -o response.json
```

Capture `/proc/PID/io`, `/proc/PID/stat` and `/proc/vmstat` before and
after the request. Sample board memory with `nvidia-smi` every 250 ms.
Compare IDs, text, state logs and `/props`. Stop the current probe before
starting the next arm. Raw evidence is under
`build/inkling-apex-gate/driver-ab-1952`.
