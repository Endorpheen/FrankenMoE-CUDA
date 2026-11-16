# EXP-2026-11-17-882 — Apex's own architecture plan is not in the DSO, and composing it needs more than its own FFI

Found while extending the arch-registry composition: `q35_plan`, `q35_kind`, `q35_shape`, `qg_split` - the plan
for the architecture that actually runs - are **absent from `libllama.so`**. They exist in
`goose/model/qwen35moe.goose` and `qgate-split.goose` and pass the standalone harnesses
(`stack_dispatch`, `gdn_plan`, `apex_plan`, `apex_layer`), but the shipped runtime decides Apex's layer kinds
in C++.

## What happened when it was composed

The object build was straightforward: both modules generate clean C, `qwen35moe` declares exactly one FFI
(`fg158_rope_multi`, a thin `ggml_rope_multi` wrapper), and each module's `goose_init` was renamed so it does
not collide with the SLRU module's. The bridge and the rope shim compiled, the objects linked, and the DSO
built.

Loading it then failed:

```
OSError: runtime/goose-stage/lib/libllama.so: undefined symbol: fg200g_ld_u32
```

So `qwen35moe`/`qgate-split` need **more than their own declared externs** - `fg200g_*` lives in the
standalone's `integration/goose-standalone/graph-bindings.c`, which is not linked into this DSO. The
dependency is real and not visible from the module's `extern` list alone.

## Reverted immediately

The DSO was broken at load, so the link change was backed out at once and the library rebuilt
(`5638176` bytes, loads again). Apex re-verified: reference hash `c2ff42cc1219`, decode 39.25.

Leaving a runtime that cannot load while investigating would have taken out the only working path.

## What the next attempt needs

A dependency walk rather than a guess: for each of the two modules, list every `U fg*` symbol its generated
object references, and satisfy that set - not just the `extern` declarations. `fg200g_*` is the graph-builder
bridge, which suggests the plan module reaches into graph construction that the working runtime already does in
C++; that overlap is worth understanding before more symbols are linked in.

The composition is therefore a bounded, well-defined task with a known missing set, and it was not a one-line
change.
