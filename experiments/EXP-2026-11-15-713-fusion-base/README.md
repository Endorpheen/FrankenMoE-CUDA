# EXP-2026-11-15-713 — the base library was the wrong one: fusion now present

Base: `d94e3067`.

## 1. Result

| run | prefill tok/s | decode tok/s |
|---|---:|---:|
| 1 | 2163.38 | 30.50 |
| 2 | 2359.66 | 34.08 |
| 3 | 2334.57 | 36.23 |
| median | **2334.57** | 34.08 |
| reference | 2228.76 | 41.62 |

prefill **2334.57 median, 2359.66 best - 4.7% to 5.9% above the reference.**
Content sha256 unchanged in all runs.

## 2. The find

Two `libggml-base` builds sit in the same directory, and we were loading the wrong one:

| file | sha256 | `fg160_query` |
|---|---|---|
| `libggml-base.so.0` (was loaded) | `8cce13c85f19` | **0 definitions** |
| `libggml-base-scheduler-bafe3955c9ea` | `e213fa19e169` | 1 definition |

`loaded-library-hashes.json` from the reference run names
`libggml-base-scheduler-bafe3955c9ea` as the base library it used. That file is the one that
provides `fg160_fusion_enabled` and `fg160_query`, which the accepted CPU library imports as
undefined symbols. Loading the other base build left those imports bound to nothing that could
ever return true, so the fusion route could not run however the CPU library was wired.

`libggml-base.so.0` now points at the scheduler build, `e213fa19e169`, matching the reference
exactly.

## 3. What did NOT change

`FG148_QUANT`, `FG150_EXPERT` and `FG160` still print nothing in the log. The fusion *symbol* is
now resolvable and prefill improved, but the quantised-dot and expert routes remain unreachable -
consistent with `PERF-INVENTORY.md`, where those entry points have no caller inside the CPU
library at all.

So the fusion fix is real and measurable, and it is not the decode fix.

## 4. Where decode stands

34.08 median against 41.62, i.e. 82%. Unchanged in substance from EXP-712 (34.27). The three
remaining candidates are the ones already named: `FG148_QUANT[9]`, `FG149_POOL[6]`,
`FG150_EXPERT` - all exported-but-unreachable in the accepted CPU library.

## 5. Honest summary of the speed work so far

| change | prefill | decode |
|---|---:|---:|
| EXP-708 first run | 855.71 | 18.21 |
| + correct `libggml-cpu.so.0` (EXP-709) | 2052.42 | 33.62 |
| + correct CPU plugin file (EXP-710) | 2222.38 | 34.95 |
| + warm-run median (EXP-712) | 2279.72 | 34.27 |
| + correct `libggml-base` (this card) | **2334.57** | 34.08 |

Every step was a packaging error, not a missing Goose implementation. The pattern is worth
stating plainly: this tree contains several builds of the same library names, and the loader
picked the wrong one twice for `libggml-cpu` and once for `libggml-base`. Each fix was found by
comparing against `loaded-library-hashes.json`, which is the artefact that makes this checkable.
