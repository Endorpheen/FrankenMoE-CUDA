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

## Free/unregister are Goose by code, unproven by witness

Checked the teardown side:

| symbol | call site |
|---|---|
| `fg196p_plan_unregister` | `shared-cuda.cu:6039` and `:6054` inside `ggml_backend_cuda_unregister_host_buffer` |
| `fg195v_plan_free` | `shared-cuda.cu:764`, computing `pool_used` from the freed pointer |

So the return path is also decided by Goose: both the host-buffer deregistration and the VMM free accounting go through plan
functions, not native bookkeeping.

**But**: the accepted run log shows only `FG195V_CUDA_VMM_POOL` and `FG196P_CUDA_PINNED`, each once. There is no witness for
`fg195v_plan_free` or `fg196p_plan_unregister` in the captured run, which is consistent with teardown output being lost when
the server is stopped, but it means the free side is "in Goose by code, not proven by execution".

Revised remainder for P3-17, final for this pass:

| aspect | owner | evidence |
|---|---|---|
| reserve/mapping/actual size (pinned host) | Goose | witness |
| fail decision (zero plan) | Goose | code |
| pool extent and usage (VMM) | Goose | witness |
| free accounting and host deregistration | Goose | code only |
| device reset, asynchronous lifetimes | native | - |
| "pinned ring" clause | absent | not present |

Nothing reclassified to done: the free side needs a teardown witness, reset and async lifetimes are still native, and the
stale clause needs a decision.

## Teardown witness attempt: inconclusive for these two modules

Ran the accepted profile, issued one short request, stopped the server with SIGINT and captured the log (384 lines). Teardown
output does print - `FG155_GEOMETRY`, `FG156_CONSTRUCTORS`, `FG157_CONTEXT_CORE`, `FG158_GRAPH_BUILDERS`, `FG160_SHARED`
all appear at the end - so the capture path works.

But `FG195V` and `FG196P` appear **zero** times in that short run, while they appear once each in the 32K run. The VMM/pinned
paths are exercised by the large allocation that a 32K prefill makes, not by a 4-token request. So this attempt did not
produce the free/unregister witness; it needs a 32K request followed by a clean shutdown.

Recorded as an inconclusive attempt rather than a negative result, and with the reason: the missing witness is about the
workload, not about the code being absent. The free side therefore remains "Goose by code, not yet witnessed", and P3-17
keeps its status.

## Teardown witness obtained: alloc/free balance, zero native calls

Third attempt, with the runner's `LD_LIBRARY_PATH` (the first two lacked it and therefore did not load the same backend),
32K request, clean SIGINT. The witnesses print counters, not just a name:

    FG195V_CUDA_VMM_POOL alloc=9526 free=9526 grow=5 native=0
    FG196P_CUDA_PINNED   host_plan=3 host_fail=0 fallback=0 reg_plan=120 reg_fail=0 unreg=240 native=0

Reading them plainly:

| counter | value | meaning |
|---|---|---|
| VMM alloc / free | 9526 / 9526 | every mapping was released - balanced lifecycle, no leak |
| VMM grow | 5 | the pool grew five times, all through the Goose plan |
| VMM native | **0** | no native fallback call at all |
| pinned host_plan | 3 | three host-allocation plans |
| pinned host_fail / fallback | 0 / 0 | no failures, no fallback taken |
| pinned reg_plan / unreg | 120 / 240 | registrations and deregistrations executed |
| pinned native | **0** | no native path taken |

So for **both** pools the full lifecycle - plan, allocate, grow, register, free, deregister - executes in Goose with zero
native calls, and the alloc/free counts balance exactly. That is stronger than the "by code" reading I had before, and it
is measured on the accepted profile, not a fixture.

What this closes and what it does not:
- **closed**: lifecycle ownership for the VMM and pinned-host pools, with counting evidence;
- **not closed**: device reset and asynchronous lifetime tracking (no witness, and no fixture yet), plus the stale
  "pinned ring" clause which needs an owner decision.

An earlier conclusion of mine is corrected again: I had listed "free accounting and host deregistration" as code-only; it is
in fact witnessed, balanced, and native-free.
