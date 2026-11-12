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

## Failure path: the decision is already Goose's

Read the integrating source (`EXP-196/source/shared-cuda.cu`, which `build_integrated.sh` places into the CUDA backend):

    if (goose) {
        fg196p_plan_host_alloc(..., plan);
        if (plan[0] == 0) { return nullptr; }        // Goose decided against allocating
        ...
    }
    // "The fallback decision is Goose's; the allocation itself is ggml's."

So for the pinned-host path the failure decision is **already owned by Goose**: a zero plan short-circuits to `nullptr` and
the host allocation never happens. The module even documents the split - decisions in Goose, the `cudaMallocHost` /
`cudaHostRegister` calls themselves are primitives.

That is a concrete advance over the earlier "planning only" reading, and it narrows the remainder:

| aspect | owner |
|---|---|
| how much to reserve / pin (plan) | Goose, witnessed |
| whether to fail (zero plan -> nullptr) | Goose, by code |
| the allocation call itself | ggml primitive |
| pool growth/reuse bookkeeping | native |
| device reset, async lifetimes | native |

Nothing is reclassified: P3-17 stays IN_PROGRESS because growth/reuse/reset/lifetimes are still native, but the failure
decision no longer belongs on the "not ported" list, and I would have missed that by scanning symbols instead of reading the
call site.

## VMM: the plan carries pool growth too

Same read applied to `fg195v_plan_alloc` (`EXP-195/source/shared-cuda.cu:645`):

    fg195v_plan_alloc(size, pool_used, pool_size, granularity, plan);
    reserve_size = plan[0];  ptr_off = plan[1];   pool_size = plan[2];
    pool_used    = plan[3];  actual_size = plan[4]; map_off = plan[5];

Six outputs, and they include `pool_size` and `pool_used` - i.e. **the new pool extent and the updated usage are Goose
decisions**, not native bookkeeping. The native `else` branch is the no-Goose path with its own 128-byte alignment and
`size > avail` test.

That corrects my own earlier listing: I had put "pool growth/reuse bookkeeping" in the native column. For the VMM pool it is
not - growth is computed by the Goose plan and only the `cuMemMap`/`cuMemCreate` calls are primitives.

Revised remainder for P3-17:

| aspect | owner |
|---|---|
| reserve size, mapping offset, actual size (pinned host) | Goose |
| fail/no-fail decision (zero plan -> nullptr) | Goose |
| pool extent and updated usage (VMM) | Goose |
| the allocation/mapping calls themselves | primitives |
| device reset, asynchronous lifetime tracking | native |
| the "pinned ring" clause | absent from this substrate |

So the item is closer than the registry says, and the remaining native part is now narrow and specific: reset and async
lifetimes, plus the stale clause.
