# EXP-2026-11-24-1852 — The staging ring is one-deep by construction; a third plane has nothing to hold

Status: **ANALYSIS, no change adopted.** Baseline `37ac0f1d` kept.

## What the accepted code actually does

`prepare_prefill_plane()` in the accepted driver implements a **one-deep** pipeline:

1. before the projection, the NULL stream waits on `prefill_ready`, so the previous
   prefetch write has landed;
2. the current plane's misses are uploaded;
3. the *alternate* plane of the same layer is copied into `next_slab = 1 - slab` on the
   expert stream, fenced by `prefill_release` recorded on NULL, and `prefill_ready` is
   recorded on the expert stream;
4. the next projection consumes it (`prefetched`), and prepares the following one.

There is exactly one `prefill_ready` and one `prefill_release` event, and the slot is
hard-coded as `1 - slab`. That is why changing `planes` from 2 to 3 produced the
single-buffer fill count: both writes went to `1 - slab`, so the spare plane was never
used as a distinct buffer and the reuse fell back to the demand path.

## Why a correct three-plane ring would still not help

The plane that can be staged is the one the **next** projection will ask for. In a
prefill that set is produced by the next layer's own router, which runs inside that
projection: at the time the driver stages ahead it knows only the immediately following
plane, because the graph visits gate, up and down of one layer in sequence.

Staging two planes ahead would need the wave identity of the projection after next,
which does not exist yet. A third plane would therefore hold nothing the runtime can
name — which is the same dependency already measured twice: EXP-1839 showed that more
waves of slack on the copy-stream fence change nothing, and EXP-1840 showed that a
route *prediction* cannot cover a prefill layer's demand (42.6-44.8 % of demands,
+22 % traffic, no gain).

So the third buffer is not blocked by the `1 - slab` arithmetic alone; correcting the
arithmetic would give a correct ring that still carries one plane of useful work.

## What this closes

| host-side knob | measured |
| --- | --- |
| staging depth 1 | prefill −3.9 %, fills +24 % |
| staging depth 3 (as implemented) | fills +24 %, no gain |
| staging depth 3 (corrected ring) | nothing to stage two ahead — analytic |
| cache bank splits | +26-27 % fills, no gain |
| one-touch SLRU off | decode −6 % |
| prefetch prediction in prefill | +22 % traffic, no gain |

The accepted configuration is the local optimum of every one of these.

## Consequence for the target

Prefill stays at the verified 3098-3112 tok/s band (target 3500). Every host-side
choice that the current architecture exposes has now been measured, and the remaining
time is expert traffic at the 22.5 GB/s link plus kernels, with the transfer and compute
phases serialised by the route dependency.

A gain from here needs a change to what the runtime *must* do rather than how it
schedules it: fewer expert bytes per request (a larger cache on a larger card, or a
model with a smaller expert set), or a prefill execution scheme that avoids
materialising each layer's full routed set. Both are outside a targeted host change,
and per rule 40 they would be new baselines, not experiments on this one.

## Artefacts

- `cand/split/src/cuda-driver.c` lines 2320-2350 (layout, `planes`), 2909-3010 (the
  one-deep pipeline, `next_slab = 1 - slab`, the event pair).
- `runs/buf3-095542` — the three-plane arm whose fill count equals the single-buffer one.
- `runs/singlebuf-095410`, `runs/split1735-094856`, `runs/split1340-095028`,
  `runs/notouch-095236` — the other arms of this cycle.
