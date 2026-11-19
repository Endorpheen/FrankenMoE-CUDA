# P9-04: row streaming, router hooks and I/O-compute overlap - what exists

Checked 2026-11-19 on `main`, text level, no build.

## What is composed

`goose/model/hook-rules.goose` is in the composed unit (`build-main-runtime.sh`, one entry) and
exports three doors:

```
rh_is_moe_node      rh_parse_tail      rh_match_layer_node
```

with private helpers `rh_at`, `rh_prefix`, `rh_tail`.

## Who calls them

**Nobody.** No source under `vendor/flint/src`, `runtime/` or `integration/` references any of the
three names. The module decides what a MoE node is and how a layer name parses, and no host asks it.

## The contract the requirement names

`rh_topk_plan` and `rh_ask_plan` exist **only inside `hook-rules.goose`** (and in one staged copy
under `goose/json/s1-6-stream-staged/`). There is no host-side declaration, no implementation, and
no caller - so the "rewrite `on_eval` against `rh_topk_plan` / `rh_ask_plan`" item has a contract
on the Goose side and nothing on the host side to rewrite yet.

`on_eval` itself is not present in the compiled sources either: searching
`vendor/flint/src` and `runtime/reconstructed/src` for `on_eval` returns no definition and no call.

## Row streaming and overlap

The native row-source and prefetch machinery that P9-04 lists (row slices, readiness, I/O-compute
overlap, cancellation, read errors, resource refusal) is not under a name this check can follow -
`row_source`, `row_stream` and `prefetch_row` do not appear in the MoE cache source either. So the
requirement's inventory cannot be matched to code by name from here, and this document does not
pretend otherwise.

## What this means

P9-04 is not "half wired". It is a contract defined in Goose with no host side yet: the doors exist,
nothing calls them, and the `on_eval` rewrite the requirement describes has no `on_eval` to rewrite
in the compiled sources.

Closing it needs the host side built first - declare and call the two plan doors from wherever the
router hook lives in this tree, then gate the row slices and the overlap order against the native
behaviour. Locating that call site is the next step, and it is a question for the tree (where does
the MoE hook fire), not for this document.
