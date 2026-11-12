# P3-17 restatement for the substrate that actually runs

Status: evidence complete, one owner decision needed. Nothing here reclassifies the item by itself.

## What the requirement asks, and what exists

| clause | status in this substrate | evidence |
|---|---|---|
| reserve size, mapping offset, actual size (VMM pool) | Goose, witnessed | `fg195v_plan_alloc` returns all six outputs including pool extent and usage; `FG195V_CUDA_VMM_POOL alloc=9526 free=9526 grow=5 native=0` |
| host plan, refuse/fallback decision (pinned) | Goose, witnessed | `fg196p_plan_host_alloc`; `FG196P_CUDA_PINNED host_plan=3 host_fail=0 fallback=0` |
| pool growth / reuse bookkeeping | Goose | computed by the VMM plan; only `cuMemMap`/`cuMemCreate` remain primitives |
| free accounting, host deregistration | Goose, witnessed | `fg195v_plan_free`, `fg196p_plan_unregister`; free 9526 of 9526 alloc, unregister 240 of 120 registrations |
| "device reset" | **not present in the backend** | both device interfaces set `/* .reset = */ NULL` -- flint `ggml/src/ggml-cuda/ggml-cuda.cu:923` and `:1674`, accepted line `EXP-196/source/shared-cuda.cu:956` and `:1746`; no `cudaDeviceReset` anywhere in the CUDA sources |
| "asynchronous lifetime tracking" | **not present** | no `cudaMallocAsync`, `cudaFreeAsync` or `cudaMemPool*` in `ggml/src/ggml-cuda`; the only `cudaLaunchHostFunc` is inside `#if 0` (ggml-cuda.cu:5291); frees are synchronous `cuMemUnmap`, `cuMemRelease`, `cudaFreeHost`, `cudaHostUnregister` |
| "pinned ring" staging | **not present** | recorded by the audit: no ring in the flint tree, `GGML_EXPERT_PINNED_RING` absent from the build |

So two of the three remaining clauses describe mechanisms this backend does not have, and the third was already
recorded as stale. The live lifetime decisions are all Goose and are balanced on the accepted profile.

## Proposed restated criterion (for the owner to accept or reject)

> P3-17 (restated): Goose owns the pool policy on the executed path -- VMM reserve size, mapping offset, actual size,
> pool extent and usage; pinned host plan with its refuse and fallback decisions; free accounting and host
> deregistration -- with a balanced allocation/free lifecycle and zero native fallback on the accepted profile. The
> allocation, mapping and transfer calls themselves remain primitives. Device reset, asynchronous lifetime tracking and
> the pinned-ring staging clause are out of scope for this substrate because the backend exposes none of the three
> (proofs above); reintroducing any of them is a design change, not a porting task.

What this would close: the item, with the evidence already collected plus the fixture in EXP-260.

What it would not do: claim anything about a reset path or an async allocator, because there is no such path to own in
this tree.

## If the owner prefers the other option

Reintroducing device reset or a stream-ordered allocator is a deliberate mechanism change: it would need its own
design, its own failure-injection cases, and a new Apex regression. It is not implied by the current runtime and it is
not needed by the accepted profile.

## Note on the fixture

The shared lifetime fixture (EXP-260) already covers the live decisions with values, not just zero/non-zero: pinned
plan (normal, disabled, zero, huge), result, fallback, register and unregister; VMM alloc into an empty pool, with room
left, rounded growth, exact fit, free to the new top and free of the whole pool. 51 checks, 0 failures, two mutations
caught. Teardown refusals implemented as `ensure() -> abort` are printed as contracts rather than driven.
