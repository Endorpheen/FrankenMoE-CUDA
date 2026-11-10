# `mm_ids_helper` width scope — the "other widths" item is moot for Apex

The plan carried an open item: "other `n_expert_used` widths for `mm_ids_helper` (only the `<int)8>`
specialization is ported)". Checked against the source; the item as written does not describe a gap
in the Apex profile.

## What native does

`mmid.cu` has one kernel templated on `n_expert_used_template` with two shapes:

- `template == 0` — the **generic** implementation, reads `n_expert_used_var` at runtime and loops
  `for (iex = threadIdx.x; iex < n_expert_used; iex += warp_size)`;
- `template == 8` (and other powers) — a **specialised** implementation that pads the slot count to a
  power of two and assumes a warp holds a whole number of token slots.

So arbitrary widths are handled by native's `<0>` instantiation, not by a separate kernel.

## What was ported

EXP181 hooked the route onto the `<8>` instantiation, which is the one the Apex profile selects
(`n_expert_used_template = 8` per the census). The `<0>` generic instantiation is still native.

## Consequence

For `n_expert_used != 8`, native's generic path runs. That is **correct and unported**, and it does
**not** execute in the fixed 32K/128 Apex profile. Porting it would add no live coverage — the same
category as PAD/ROLL.

Worth stating separately because it is easy to conflate: the Goose implementation itself takes
`n_expert_used` as a **runtime parameter** (device.goose line 63, used at line 76), so it is not
structurally limited to 8. But the gate only tests 8 — all five fixture cases use `n_used = 8` — so
"the Goose module handles other widths" is **untested**, not established. If the dispatch were ever
widened to route other widths to it, the gate would have to grow first.

## What would make the claim

Extend `tests/gpu-fixture.cu`'s `CASES` with `n_used` of 2, 4 and 16 and route them to the Goose
entry directly rather than through the width-gated dispatch. Until that exists, this document claims
scope, not coverage.
