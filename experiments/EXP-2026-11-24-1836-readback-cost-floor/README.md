# EXP-2026-11-24-1836 — A 2 KB read costs 5 us when the device is idle, so batching cannot help

Status: **MEASUREMENT.** Baseline `37ac0f1d` unchanged (prefill 3122.4,
decode 78.225). No code change adopted.

## The question the next step depends on

EXP-1835 left 62 400 mid-graph reads of about 2 KB, costing 12.286 s over three
requests, roughly 200 µs each. The proposed treatment was to gather the reads of one
plan into a single pinned transfer. That only pays if the ~200 µs is *call
overhead*; if it is the wait for the device to produce the tensor, gathering cannot
remove it and would only risk correctness.

## The measurement

A standalone micro-benchmark (`dtoh-bench.c`, `result.txt`, compiled with
`gcc -O2 ... -ldl`, run on the same driver and card, device idle):

```
2 KB DtoH, device idle, 2000 calls:
  pageable blocking       4.97 us/call
  pinned blocking         4.22 us/call
  pinned async+event      4.20 us/call
  16 gathered + 1 ev     26.76 us/16 calls (1.67 us/call)
```

The floor for one 2 KB read is **4.97 µs** pageable and 4.20 µs pinned, and even a
gathered batch of 16 with one event costs 1.67 µs per read. The model run measures
**~200 µs per read**, which is therefore **40 times the transport cost**: it is the
wait for the device.

## Consequence

- Gathering the reads of a plan cannot help: 21 312 dependent waits per request
  remain dependent regardless of how their transfers are submitted. The idea is
  closed by measurement, before any code was written for it.
- The prefill is a **chain of ~21 000 dependent steps**: the host issues a couple of
  kernels, waits for the device to produce a tensor, reads it, decides the next
  step. EXP-1835 showed those tensors are genuinely consumed by the host plan
  scheduler, so they cannot be skipped.
- The budget closes arithmetically: ~4.3 s of these waits, ~3.7 s of kernels and
  ~4.2 s of expert traffic at the 21.7 GB/s link ceiling, against a 10.5 s request.

## What this rules out for the 3500 target

Every host-side and transport-side lever is now excluded by measurement, the last
one with a hard floor rather than an inference:

| lever | evidence |
| --- | --- |
| copy submission and batching | 5 variants, batch DMA, gathering: neutral or worse |
| pinned staging (source and destination) | −3.1 % and neutral, gate fixed and re-measured |
| host waits | async plan staging −0.03 %, deferred waits 3103.7/3098.4 |
| expert prefetch depth | already k=8, 0.36 s of host time, 0 discarded |
| skipping host reads | output changes, gate fails |
| readback transport cost | 5 µs against 200 µs observed, so the rest is dependency |

The remaining route to 3500 is to reduce the *number of dependent steps* or the
device work inside them — a scheduler/executor redesign, or more VRAM so the expert
set fits and each step gets shorter. Neither is a tuning change, and neither can be
validated by the end-to-end text alone.

## Artefacts

- `experiments/EXP-2026-11-24-1836-readback-cost-floor/dtoh-bench.c` — the benchmark
  source, reproducible on any card.
- `experiments/EXP-2026-11-24-1836-readback-cost-floor/result.txt` — this run's
  output.
- `runs/skiprb-080745` — the readback-skip arm and its gate failure (EXP-1835).
- `runs/actb-080235` — per-site readback split, bounce active (EXP-1834).
