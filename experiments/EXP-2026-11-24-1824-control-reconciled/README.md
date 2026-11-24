# EXP-2026-11-24-1824 — Control reconciled: 3122.4 and 2686 are the same binary, different request shape

Status: **MEASUREMENT.** Baseline `37ac0f1d` unchanged.

## The two figures

| figure | where it comes from |
| --- | --- |
| 3122.4 tok/s (10557.5 ms) | `runs/cf-B3-20261123-205433`, binary `37ac0f1d`, acceptance request **32768 prompt + 512 predicted**, second request of the block, `GOOSE_DECODE_SPLIT=4/COUNT=1`, ctx 262144, cache 6290 |
| 2686 tok/s (12.2 s) | diagnostic runs of the same binary and profile with **`n_predict` = 1** (`runs/drvtime-235827` 12312.1 ms, `runs/ctx128c6290-020744` 12208.5 ms) |

## The controlled comparison

One server, one binary (`37ac0f1d`), identical argv and env, four requests in
order, per-request processor time read from `/proc/<pid>/task/*/stat`:

| request | `prompt_ms` | tok/s | wall | CPU | busiest thread |
| --- | ---: | ---: | ---: | ---: | --- |
| 32768/1, warm-up | 12445.1 | 2633.0 | 12.55 | 12.59 | 12.49 s |
| 32768/1, measured | 12321.4 | 2659.4 | 12.42 | 12.42 | 12.37 s |
| 32768/512, warm-up | 12280.8 | 2668.2 | 19.01 | 19.11 | 18.95 s |
| **32768/512, measured** | **10570.0** | **3100.1** | 17.28 | 17.40 | 17.24 s |

Three facts:

1. The 3122.4 figure is the **second request of the 512-token shape**. In this
   block the same shape reads 3100.1 tok/s (10570.0 ms), so the number is
   reproducible to about 1 %.
2. **`n_predict` = 1 is 1.75 s slower** (12321.4 against 10570.0 ms) with the same
   binary, profile and request prefix. The warm-up shape therefore changes the
   cache state the measured prefill meets; the earlier diagnostic figures are not
   comparable with the acceptance figure, exactly as the review said.
3. The process has **five threads and one of them does all the work**: 12.4-19.0 s
   of processor time on the main thread, the other four between 0.00 and 0.16 s.
   CPU equals wall in every request, so the prefill is a single-threaded,
   CPU-bound chain — the GPU is not the critical path, the host thread is.

## What this means for the 3500 target

From 3122.4 to 3500 needs 1.13 s of the 10.57 s request, 10.7 %, removed from one
thread. The measured contents of that thread are the CUDA API (5.64 s in the
traced request) and its own code; the largest single API item is 1860
synchronising plan copies at 4.9 s (`cuMemcpyHtoD_v2`, 2.6 ms each).

That 4.9 s is the one segment large enough to pay for the target, and three
attempts to move it have failed (packet ring -2.95 %, `decode_upload` ring +6.2 %
at 4096 tokens, windowed-upload stream -2.4 %), which means the cost is not the
call we issue but the driver's pageable staging behind it. Until a run shows the
staging time itself falling, the measured reserve for that change is zero.

## Not yet explained

- 88.3 % of request processor samples sit in `libcuda` while the whole API
  histogram sums to 5.64 s: the difference is driver-internal work that my
  per-call timers do not cover.
- The 1.75 s difference between the `n_predict` = 1 and 512 shapes is measured but
  not attributed: it is a cache-state effect inside the prefill, and it has to be
  understood before any prefill figure is compared across requests.

## Artefacts

`runs/ctrl-065141` (four requests, per-thread counters, requests and responses),
`runs/cf-B3-20261123-205433`, `runs/drvtime-235827`,
`runs/ctx128c6290-020744`.
