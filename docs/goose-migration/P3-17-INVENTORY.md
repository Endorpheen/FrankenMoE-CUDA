# P3-17 inventory: GPU/pinned/VMM pools and transfer/residency policy

Status: inventory step, item stays IN_PROGRESS and its "pinned ring" clause is stale (see the audit's stale-clause table).

## Already Goose, on the executed path

From `libggml-cuda` symbol scan plus witnesses in the accepted run log:

| symbol | meaning | witness |
|---|---|---|
| `fg195v_plan_alloc` | VMM allocation plan (virtual range, physical mapping granularity) | `FG195V_CUDA_VMM_POOL` in the log |
| `fg196p_plan_host_alloc` | pinned host allocation plan | `FG196P_CUDA_PINNED` in the log |
| `fg196p_plan_host_alloc_result` | result/decision of that plan | same |

So the two decisions the requirement cares about most - how much virtual space to reserve and what to pin on the host - are
already Goose-owned and observed executing.

## Still native

- pool growth/reuse bookkeeping in the CUDA backend beyond the planning decision;
- OOM handling, device reset, asynchronous lifetime tracking;
- the "pinned ring" itself - absent from this substrate, not native-vs-Goose but simply not present.

## What closing P3-17 would need

1. an explicit decision on the ring clause (restate or reintroduce), same as P1-09/P1-10;
2. for the live part: move the remaining pool growth/reuse and lifetime decisions behind Goose decisions with the
   OOM/resize/reuse/cancel/reset checks the criterion lists, then prove them with a failure-injection fixture - not by
   asserting that the planning functions exist.

Nothing here is claimed done: symbols plus witnesses show planning ownership, not lifecycle ownership.
