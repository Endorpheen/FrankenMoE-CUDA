# EXP-2026-11-15-712 — prefill now exceeds the reference; decode is the only gap

Base: `4621e69a`. Four consecutive requests, one server, identical conditions.

## 1. Result

| run | prefill tok/s | decode tok/s |
|---|---:|---:|
| 1 | 2033.68 | 34.36 |
| 2 | 2271.99 | 35.75 |
| 3 | 2287.44 | 33.50 |
| 4 | **2291.50** | 34.18 |
| median | **2279.72** | 34.27 |
| reference 2026-11-13 | 2228.76 | 41.62 |

**prefill: median 2279.72, best 2291.50 - above the reference by 2.3% to 2.8%.**
**decode: median 34.27 - 82.3% of the reference, and this is now the only gap.**

Content sha256 identical in all four runs and identical to the reference
(`9d90a010...1ba7560`), so the speed came without changing the answer.

## 2. What this changes about the earlier readings

The first request after a server start reads 2033-2145; the second onward reads 2272-2291. That
is a page-cache and CUDA-context warm-up effect, not a code difference. EXP-710 reported 2222.38
from a single warmed run and EXP-711 reported 2145.95 from a first run - both were measuring
different points of the same warm-up curve.

Reporting the median of four and the best of four is the honest form, and both are above the
reference.

## 3. Where decode stands

`decode` is 34.27 against 41.62. The inventory in `PERF-INVENTORY.md` names the three hot paths
that are unreachable in the accepted CPU library - `FG148_QUANT[9]` (482 400 reference calls),
`FG149_POOL[6]` (316 164) and `FG150_EXPERT` (135 360) - and establishes that the binary which
produced those counters is not on this machine.

Note that prefill exceeding the reference while decode does not is consistent with that finding:
prefill is dominated by large batched matmuls that the working routes already cover, while
decode is exactly where the per-token quantised dot and expert selection would show up.

## 4. Not claimed

That decode is fixed. It is not. The next step remains the one named in the inventory: generate
the CPU library from the Goose sources that already exist here
(`goose-cpu-q4k-dot`, `goose-cpu-pool`, `goose-moe-control`) with those routes wired, then
measure. That build removes C++ rather than adding it, which is what the mandate requires.

## 5. Reproducibility

`runtime/goose-stage/run-apex.sh`, unchanged; four requests in sequence on one server; responses
saved per run.
