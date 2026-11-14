# EXP-2026-11-14-577 — qwen35moe builder (P6-05 architecture 1)

Native counterpart: `src/models/qwen35moe.cpp` (Apex profile: 35B-A3B, 40 trunk
layers + 1 MTP, 256 experts × 8 used, hybrid recurrent/attention). Base commit:
`96e2fb91`.

## What this slice covers

`source/qwen35moe.goose` is the plan/shape half of the builder: the per-layer
kind decision (attention vs gated delta net on the hybrid interval, the plus-one
marker), the MTP layer in the plan, the per-layer marker direction, negative
index rejection, GDN nodes, qk-norm width, routed-expert and shared-expert
widths, the router table, the attention QKV blocks and the output tensor.

`source/base/model_graph.goose` is the accepted model-graph slice without its
`fn main()` (canonical `goose/model/goose-model-graph.goose`; the only
difference is the entry point).

## Gate

`tests/build.sh` builds both modules with `--standalone` at `-O1` and runs the
ten original cases plus an extended block:

```
FG579_ARCH1 cases=10 values=26 failures=0
FG579_ARCH1_EXT cases=43 values=149 failures=0
```

The original ten cases are kept verbatim; the extended block adds the classes
the original corpus could not reach: a lost output tensor, the shared-expert
flag, an inverted per-layer marker and a dropped MTP layer.

## Mutations

`tests/mutate.py 1|2` holds 12 semantic mutations in two halves; each rewrites
the source, rebuilds through `tests/build.sh` at `-O0` in its own build
directory, and requires the gate to fail.

```
python3 tests/mutate.py 1   # 6/6 caught
python3 tests/mutate.py 2   # 6/6 caught
```

The baseline is rechecked after each half and reports `FG579_ARCH1 cases=10
values=26 failures=0`.

## Recorded limits

- The gate is structural: it checks plan and shapes against a transcription of
  `qwen35moe.cpp:54-146`; no native library is linked and no model runs.
- The shape half only; graph execution, kernels and weights are out of scope.
