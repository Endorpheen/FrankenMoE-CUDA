# C2-04: extending the runtime with another architecture, as actually done for Qwen4Exp

Recorded 2026-11-19 from the Qwen4Exp integration, because that work is this requirement's subject:
a new family joins the shared runtime without a second backend and without duplicating the base.

## The pattern that worked

`qwen4exp`, `q4m-metadata` and `q4e-expert-layout` are now in `tools/build-main-runtime.sh`
alongside `q4t-tensor-map` and the rest of the unit, and the composed unit typechecks (4540
specializations, 7 structs, ok). Nothing was forked: the slices import `base.model_graph`, which the
composer aliases to the unit's `goose-model-graph.goose`, so the expert widths and the dense spans
are the shared ones.

Three things had to be true, and each was learned by the composer refusing the set:

1. **Per-family constant names.** `P_*`, `K_*` and `T_*` carried different values per architecture
   (qwen4exp `P_HAS_OUTPUT` 26, gemma3 8, deepseek32 21). A flat unit holds one value per name, so
   the family prefix (`Q4P_`/`Q4K_`/`Q4T_`, 189 references) is what makes an extension composable.
   No value changed.
2. **One abort primitive, bound locally.** The graph's `fail` is renamed to `fgg_fail` at
   composition, so a module that imports the graph cannot call `fail` and still link. Each new
   module binds `fg200g_fail` under its own Goose name (`q4_abort`).
3. **The gate builds from `goose/`.** Moving the experiment's 43-case gate into `tests/` immediately
   found both defects above, because the experiment had only ever built its own isolated copies.

## What protects the existing runtime

- `tests/regression/accepted-components` - every accepted component is still in the shipped library
  (fast, seconds);
- `tests/regression/symbol-loss` - nothing that existed in the previous accepted build disappeared
  (rule 33);
- `tests/integration/unwired-modules` - no module exports a decision and sits outside every unit;
- `tools/check-cumulative-baseline.py` before any acceptance measurement.

Those three tests plus the composer's own conflict report are the mechanism by which "extend
without breaking Apex" is checked rather than remembered. The Apex correctness gate on real weights
is still a model run, and it has not been made since the Qwen modules joined the unit.

## What is not claimed

That a Qwen model runs. No Qwen GGUF has been loaded at any point; the integration is proven at the
composition and typecheck level, and the slices are proven against their transcriptions. C1-03,
C1-04 and C1-05 carry the rest.
