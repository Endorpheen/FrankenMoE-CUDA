# EXP-2026-11-24-1826 — The two unexplained intervals, attributed as far as the data goes

Status: **MEASUREMENT.** Baseline `37ac0f1d` unchanged (prefill 3122.4,
decode 78.225).

## Interval A: is the mmap registration per request?

Hypothesis: 120 registrations of 19.5 GB costing 9 s are what put 88.3 % of the
request's processor samples in `libcuda` while the API histogram totals 5.64 s.

Measured with a counter behind `cuMemHostRegister_v2`, printing at every `/props`:

```
before request 1: 120 calls, 19.50 GB, 9.049 s total; last 0.46 s ago
after  request 1: 120 calls, 19.50 GB, 9.049 s total; last 13.03 s ago
after  request 2: 120 calls, 19.50 GB, 9.049 s total; last 25.43 s ago
```

**Refuted**: the counter is unchanged across two requests, so the registration is a
one-time start-up cost of 9.05 s, paid before the server answers `/health`, and it
is not part of any request. The request's `libcuda` share is therefore not
registration, and the 5.4 s gap between samples and API duration stays open: it
is driver-internal work on the calling thread that the API entry timer does not
cover.

## Interval B: the 1.75 s request-shape effect

From EXP-1824, same binary and profile, four requests in order:

| request | `prompt_ms` | tok/s |
| --- | ---: | ---: |
| 32768/1 warm-up | 12445.1 | 2633.0 |
| 32768/1 measured | 12321.4 | 2659.4 |
| 32768/512 warm-up | 12280.8 | 2668.2 |
| 32768/512 measured | 10570.0 | 3100.1 |

Reproduced again in this run: 32768/1 after a 32768/1 warm-up reads 12355.6 and
12227.0 ms (2652.1 and 2680.0 tok/s).

The effect is therefore a **warm-up shape effect on the cache state the measured
prefill meets**: only a preceding request that ran 512 decode steps leaves the
expert cache in the state that makes the next prefill 1.75 s faster. It is not a
lever: the acceptance protocol already uses that shape, so the 3100-3122 tok/s
figures already include the benefit, and a real request that follows no such
warm-up cannot claim it.

## What is left

Neither interval yields a segment of 1.13 s that can be taken:

- A is a start-up cost, outside every request;
- B is already counted in the baseline.

The one measured segment of that size, the 4.9 s of synchronising plan copies, has
been attacked five ways with no gain (EXP-1810, 1811, 1813, 1820, 1825), and the
copy count itself cannot be reduced (coalescing ratio 1.00, EXP-1821).

## Artefacts

`runs/reg-065754` (the counter at three points, two requests), `runs/ctrl-065141`
(the four-request shape comparison).
