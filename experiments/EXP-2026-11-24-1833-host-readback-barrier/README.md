# EXP-2026-11-24-1833 — The barrier is 21 312 host readbacks per request, not prefetch

Status: **MEASUREMENT, candidate rejected.** Baseline `37ac0f1d` unchanged
(prefill 3122.4, decode 78.225). No code change adopted.

## The prefetch hypothesis, tested before writing code

The existing prefetch machinery was instrumented through its own counters, and it
already runs deep: `k=8`, 19 958 launches, 10 000 waves, 48 873 jobs, 0 skipped,
with **207 ms** of total wait, 55 ms of launch, 33 ms of prepare and 68 ms of submit
over two requests. The prefill counters report 960 projections, 59 856 resident
planes, 627 prefetched and **0 discarded**.

Prefetch depth is therefore not the missing 1.13 s: the whole mechanism costs
0.36 s of host time and never discards a wave. Adding depth would add traffic, not
hide it.

## The segment that does have the size

Timing every driver entry not yet covered (`GOOSE_EXP_TRACE=1`, deltas between two
`/props` calls around a warm-up and a measured 32768/512 request,
`runs/cpyt-074738`):

| entry | measured |
| --- | ---: |
| `cuMemcpyDtoH_v2` | **42 624 calls, 1.102 GB, 8.802 s** (max single call 6 ms) |
| `cuLaunchKernel` | 112 519 calls, 0.268 s |
| `cuCtxSynchronize` | 2 482 calls, 1.218 s |
| expert uploads (host side) | 285 228 calls, 0.122 s |
| allocation, mapping, events | < 0.02 s |

That is **4.4 s per request** in device-to-host reads, 21 312 calls per request,
551 MB, 25.8 KB per call — and the per-call cost of about 206 µs is *not* transfer
time.

## Pinned staging does not help, so it is queue wait

Candidate: route every DtoH through a pinned bounce buffer
(`cuMemcpyDtoHAsync_v2` into pinned staging, wait on a recorded event, then hand
the caller its bytes).

**Correction (EXP-1834):** the bounce did **not** engage in this experiment. The
gate required `cuda.event_sync`, which this profile loads only for
`GOOSE_CUDA_EXPERT_STAGE_PAGEABLE`; the counters below came from the plain
instrumentation wrapper, not from the bounce. The "pinned staging does not help"
conclusion is only established by `runs/bon3-080045` and `runs/actb-080235` in
EXP-1834, where the gate really is active. The numbers recorded here remain valid
as neutral A/B arms, but they are not evidence about pinned staging.

| run | prefill tok/s | decode tok/s | text |
| --- | ---: | ---: | --- |
| bounce, warm-up + two measured | 3116.8 / 3119.0 | 78.050 / 78.095 | `f8664331bd30` |
| first attempt, gate failed, control | 3114.0 / 3108.7 | 78.237 / 78.037 | `f8664331bd30` |

Bit-exact, decode parity, VmHWM 21 492 900 kB, swap 0, VRAM peak 10 743 MiB — and
the DtoH time stayed at 13.152 s for three requests (4.38 s each). A pinned
destination only removes the driver's staging copy; it cannot remove a wait for the
device queue that the caller's dependence imposes.

Neither does the SSM reference path explain it: with `GOOSE_SSM_REFERENCE=0` the
readback count is identical (42 624 calls, 8.755 s) and prefill reads 3130.1
(`runs/ssm-075228`).

## What that means

Each readback is a **barrier**: the host cannot continue until the device has
produced that tensor, so the prefill advances in about 21 000 dependent steps. The
request is not short of bandwidth, not short of host speed and not short of
prefetch depth; it is short of *device residency* for intermediate tensors. That is
the same conclusion the 2.8 s non-overlap of EXP-1832 pointed at, now with the
segment named and sized.

The single change that follows is not a transport change: **stop bringing
intermediate activations to the host when the consumer is a device operation**, or
at least batch the reads that the host genuinely needs, so that 21 312 barriers
become a few hundred. That touches the graph executor's tensor residency contract
and must be done as its own experiment with a differential gate, not as a wrapper.

## Rejected candidates in this experiment

- deeper expert prefetch: the mechanism already runs at k=8 with 0.36 s of host
  time and zero discarded waves;
- pinned DtoH staging: measured above, neutral, and it *proves* the cost is the
  barrier rather than the transport;
- `GOOSE_SSM_REFERENCE=0`: identical readback count, no gain.

## Artefacts

- `runs/cpyt-074738` — the DtoH and launch timers with the full budget.
- `runs/bounce-074934` — first bounce attempt, gate not yet installed (control).
- `runs/bounce2-075101` — bounce engaged, neutral, bit-exact.
- `runs/ssm-075228` — SSM reference off, same readback count.
- `runs/memtr-074321`, `runs/extr-074102` — expert-path counters (EXP-1832).
- Prefetch counters are printed by the shipped binary itself at exit; no change
  was needed to read them.
