# EXP422: Apex memory-fit policy, native vs unified

P7-10 was PARTIAL because the memory-fit policy had never been exercised on
Apex: the small stories model caps requests at its 2048-token training context
before fit acts. This experiment runs the policy on the real Apex model.

## Setup

Four server starts, no generation:

| arm | binary |
|---|---|
| native | `llama.cpp-flint-cuda/build-cuda/bin` |
| ours | `runtime/exp421/unified` (baseline `stage-run` + Goose logging) |

Both arms: Apex 35B Q4_K_M, `-c 32768`, `-ngl all`, `--cpu-moe`, `-fa on`,
KV `q4_0`, batch/ubatch 4096, 8/16 threads, `-np 1`, `-lv 4`, and `-fit on` or
`-fit off`. Each server is stopped with SIGINT after health.

## Result

All four loads reached health; all four exited cleanly.

`-fit off` is line-identical between native and ours.

`-fit on` produces the same decisions on both lines:

    common_params_fit_impl: projected to use 2893 MiB of device memory vs. 8150 MiB of free device memory
    common_params_fit_impl: will leave 5256 >= 1024 MiB of free device memory, no changes needed
    common_fit_params: fitting params to free memory took 0.41 seconds

Native reported 8126 MiB free and 5232 MiB left; ours reported 8150 and 5256.
That 24 MiB difference is the free-VRAM measurement at different moments, not
a policy difference: the projected use (2893 MiB), the margin rule (>= 1024)
and the no-change verdict are identical.

Fit changes placement on both lines (`on` lines differ from `off` lines), so the
equality is not a case of fit being inert.

## Status of P7-10

The policy is behaviourally qualified on Apex, but it is still implemented in
`common/fit.cpp` (native C++). P7-10 asks for the policy to be ported, so the
item stays PARTIAL. This experiment closes the missing evidence clause; the
remaining work is a real Goose port of the fit algorithm (memory projection,
layer/context binary search, tensor-split and buffer-override decisions), which
is a standalone task larger than a test pass.

`docs/goose-migration/P7-10-INVENTORY.md` (or the task evidence) should record
that distinction instead of marking the item DONE on behavioural parity alone.

## Reproduce

    python3 experiments/EXP-2026-11-13-422-p7-10-fit-apex/run_fit_policy.py

Requires one free Apex-capable GPU; ports 18320/18321 must be free.
