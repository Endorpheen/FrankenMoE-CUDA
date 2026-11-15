# P9-04 — row streaming, router hooks and I/O-compute overlap

Status: **PARTIAL** — 3 of 3 slices accepted; the call sites and the `on_eval`
rewrite are open.

## Slices

| slice | content | exports |
|---|---|---|
| S1 router hook rules | `is_moe_node`, `parse_layer_tail` with the 2^20 ceiling, `match_layer_node`, four weight-chain names, drop/substitute arming, node isolation, terminal-node learn/forget, workload refusal, two call-order plans | 18 `rh_*` |
| S2 overlap rules | `p0`/`two_wave`/`commit_only`, staged/wave jobs and publication (wave 2 grows the batch), drain/wait decisions, one-layer gate scope, stall union | 20 `ov_*` |
| S3 row-source selection | materialised-index transitions, ref/table verdict, session dense/rows filter, takeover, gather/materialize/scan dispatch, LRU unlink of a materialised table | 12 `rs_*` |

The experiment's `stream_hooks.goose` is a build artefact (0 exports, 3
imports); the canon holds the three source modules and the composer rebuilds the
unit.

## Evidence

- Experiment: `experiments/EXP-2026-11-15-629-p9-04-stream-hooks`.
- Canon: `goose/model/{hook-rules,overlap-rules,row-select}.goose`.
- Gate: 302 + 560 + 145 cases, 0 failures, identical lines at -O0.
- Mutations: 12/12 in two halves, each caught by a gate failure rather than a
  build error.

## Remaining

Rewrite `on_eval` to the `rh_topk_plan`/`rh_ask_plan` contract and replace the
overlap and row-source call sites. `rh_topk_plan` is a call-order contract, not
a single predicate replacement.

## Boundaries

`term_variant_` (`vector<int8_t>`) needs an adapter to `i64[:]`; the StallUnion
window is `i64[3]`; three experiments generate identical typedef headers, so
their units cannot share one translation unit. The lifecycle case drives both
sides from one shared schedule.
