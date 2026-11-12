# EXP260: shared lifetime fixture for the P3-17 pool policies, and fix #4 from the consolidated list

Two things in one item, because the fix could not be accepted without the fixture that measures it.

## 1. Defect #4 from `FIX-LIST-20261112.md` is fixed

`plan_host_alloc` in the pinned-memory policy approved a zero-byte request:

    before: size == 0, disabled == 0  ->  plan[0] = 1   (attempt the driver call)
    after:  size == 0                 ->  plan[0] = 0   (refuse, caller falls back)

Native `ggml_cuda_host_malloc(0)` calls `cudaMallocHost(ptr, 0)`, gets `cudaErrorInvalidValue`, returns `nullptr`,
and the buffer type falls back to the CPU path. The refusal takes the same route without a driver call and now matches
the VMM plan, which rejects `size <= 0` (`ensure(size > 0, 10)`). The MiB figure for the failure log is still written,
so a refused request keeps its diagnostic value.

The rule is stated in the module next to the code: a zero-byte request cannot produce a buffer, so the decision not to
attempt belongs to the plan, not to the driver's error path.

## 2. The shared fixture

`tests/lifetime_fixture.c` drives both pool policies in one binary -- the audit's point that P1-09/P1-10/P3-17 need one
lifetime fixture, not three:

| module | entries exercised |
|---|---|
| `fg196p` pinned host | plan (normal / disabled / zero / huge), result ok+fail, fallback null+ptr, register on/off, unregister ok/fail/off |
| `fg195v` VMM | alloc into an empty pool, alloc with room left, growth rounded to granularity, exact fit without growth, free to the new top, free of the whole pool |

Result:

    checks: 51, failures: 0
    FG195V_CUDA_VMM_POOL alloc=4 free=2 grow=2 native=0
    FG196P_CUDA_PINNED host_plan=5 host_fail=1 fallback=1 reg_plan=2 reg_fail=0 unreg=3 native=0

The fixture checks values, not "nonzero": reserve, pointer offset, pool extent, usage, actual size, mapping offset,
MiB thousandths, and every fallback/log flag.

Refusals implemented as `ensure() -> abort` (negative size in both modules, zero size in the VMM plan, a free that is
not at the top of the bump allocator) are printed as contracts rather than driven, because a fixture that can die is
not measuring the decision.

## The fixture was shown to fail

A green gate that cannot fail is not evidence. Two mutations, both inside the emitted code the fixture links:

| mutation | result |
|---|---|
| MiB divisor 1048576 -> 1048577 | `pinned … mib_milli` checks fail |
| 128-byte alignment -> 129 | 14 failures across `used`, `actual`, `reserve`, `pool` |

Restored, the fixture returns 51/0 again.

## Why the Apex answer cannot move

Two independent arguments, because the regression run turned out not to be reproducible:

1. **The accepted run's own counters.** `FG196P host_plan=3 host_fail=0 fallback=0`: three plans ran, none failed, no
   fallback was taken, so all three had `size > 0` and the new refusal branch is unreachable on that profile.
2. **The call site makes the change observationally equivalent.** `ggml_cuda_host_malloc` returns `nullptr` when
   `plan[0] == 0`; before the fix a zero-size request instead reached `cudaMallocHost(ptr, 0)`, which always fails,
   sets `ok = 0`, and returns `nullptr` as well. Both routes end in the same `nullptr` handed to
   `ggml_backend_cuda_host_buffer_type_alloc_buffer`, which falls back to the CPU buffer type. The only observable
   difference is the absence of one `GGML_LOG_DEBUG` line and one driver call. No numerical path can change, at any
   `size`.

**The regression run itself is not reproducible from the retained artifacts.** The accepted
`libggml-cuda-pinned.so.0.27.0` is a 220 MB DSO linked from about thirty module objects plus
`shared-cuda-goose.o`; the recorded link line (`local-test-commands.json`, 201 arguments) refers to objects that
lived in the build working directory and were not retained. Only the linkable DSOs survive. Rather than fabricate a
check, this is recorded as a limitation: a future pass that wants the run must first keep the object set, or rebuild
the integration from `source/shared-cuda.cu` on a line that can be rebuilt end to end.

## Still open in the consolidated list

Defects 1-3 (SILU scalar tail association, `clamp(NaN)` bound order, `scale(-0)` sign) are in `pointwise.goose` and are
untouched here. They need the same regenerate-with-the-qualified-compiler treatment plus
`tools/p2-01-contract/build-and-compare.sh`. P2-01 stays PARTIAL until that pass runs.

## Files

- `source/pinned-host.goose` -- fixed policy (regenerated with the pinned compiler)
- `source/pinned-bridge.c`, `source/vmm-bridge.c` -- publication only; `gg_init_globals` is renamed per module so both
  generated units can share one binary
- `tests/lifetime_fixture.c`, `build_and_test.py`, `commands.json`
