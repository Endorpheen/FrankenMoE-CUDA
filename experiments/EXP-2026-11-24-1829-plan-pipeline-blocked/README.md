# EXP-2026-11-24-1829 — Projection-level plan pipeline: engaged, crashed, defect named

Status: **NOT ACCEPTED. Three pipeline attempts, none neutral-or-better yet, defect
identified.** Baseline `37ac0f1d` untouched and intact.

## What was built

Building on EXP-1828 (the plan copy is 5.954 s of GPU queue wait against 0.011 s of
copy), the plan copy got two device slots and its own non-blocking stream:

- `cuda.mmq_tiles` is reserved as two aligned slots;
- the host fills a pinned buffer, queues `cuMemcpyHtoDAsync_v2` on the plan stream,
  records an event, and makes the null stream wait on it — **without** the earlier
  mistake of recording a begin-event on the null stream, which was what made the
  first attempt (EXP-1813) wait for the whole queue anyway;
- the kernel reads the slot of its own projection, so projection k+1's plan can
  land while projection k executes;
- `GOOSE_PLAN_PIPE=0` restores the old copy.

## The two failures

| attempt | what happened | cause |
| --- | --- | --- |
| 1 | 5319 of 5320 copies fell back to the synchronous path | the slot stride was rounded to 1 MiB while the reservation was `2 x tile_bytes`, so the second slot pointed past the buffer |
| 2 | stride aligned, pipeline active (`plan pipeline active (2 device slots)`), **first request crashed** | `CUDA error 719` (`LAUNCH_FAILED`) on the kernels, and `mmap expert unregistration failed (719)` at exit |

The second failure has a definite cause in my code: the "slot free" event is
recorded on the null stream **before** the kernels of that projection are
launched, so two projections later the slot is overwritten while the older kernel
may still be reading it. The fence has to be recorded after the launches of the
projection that consumes the slot.

## Where that leaves the lever

The lever is real and measured (6.4 s of host phase with an idle GPU, 6.0 s of GPU
phase with a spinning host, zero overlap; perfect overlap would give about
5100 tok/s and a quarter of it would close the 1.13 s the target needs). The
implementation is close but not correct: the free-fence ordering must be fixed and
the result verified with the queue-wait counter (it has to fall well below 5.954 s)
before any A/B is meaningful.

I am stopping the pipeline here for this session rather than shipping an
unverified variant: two structural failures in a row on the same mechanism mean the
next step wants a careful pass over the ordering (and probably a look at how the
graph replay path consumes `cuda.mmq_tiles`), not another blind rebuild.

## Artefacts

`runs/pipe-070820` (all-fallback build), `runs/pipe2-070917`,
`runs/pipe3-071018` (pipeline active, crash with 719), `runs/split-070555` (the
queue-wait split that motivated the work). Build trees
`/tmp/opencode/apex-pipe`, `/tmp/opencode/apex-split`.
