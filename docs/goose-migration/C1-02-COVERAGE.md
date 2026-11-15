# C1-02 — Qwen4Exp model graph and metadata

Status: **PARTIAL** — 3 of 3 slices accepted; four boundaries remain.

## What was already covered

`goose/model/qwen4exp.goose` (P6-05, EXP580) owned the layer plan and shapes:
`q4_kind`, `q4_is_ple`, `q4_plan`, `q4_shape` (52 tensor ids) — attention / gated
delta net / MTP selection, the PLE marker, and the GDN/HC/indexer/expert/MTP
shape arithmetic.

## New slices

| slice | content | exports |
|---|---|---|
| S1 metadata | 27 GGUF keys with mandatory/arity classes, PLE/HC/indexer/SSM guards in native order, recurrence from `full_attention_interval`, `n_layer == 48 → A3B` | 17 `q4m_*` |
| S2 tensor map | 52 ids with name, arity, suffix, bucket, loader flags (required/not_required/duplicated/read_lazy/skip_if_virtual) and presence | 10 `q4t_*` |
| S3 expert layout | routed/shared experts, fused `ffn_gate_up_exps` vs split probes, fused `attn_qkv` vs separate, shared widths; reuses `base/model_graph` (`n_ff_exp`, `n_ff_shexp`, `q_full_width`) | 14 `q4e_*` |

## Evidence

- Experiment: `experiments/EXP-2026-11-15-660-c1-02-qwen4exp`.
- Canon: `goose/model/{q4m-metadata,q4t-tensor-map,q4e-expert-layout}.goose`.
- Gates: 15/222 + 13/528 + 12/28 values plus a join gate 3/156, 43 cases /
  934 comparisons, 0 failures, byte-identical at -O0.
- Mutations: 36/36 (12 per slice, halves 6/6).
- The join gate checks that `q4t_present` agrees with `q4_shape` for 3 layer
  kinds × 51 ids, so the map and the plan cannot drift.

These modules are per-architecture building blocks. They compose with the
canonical base modules but not with the common runtime core (the base helpers
`fail`, `is_recr`, `crop_before_nextn` have per-arch bodies), so they stay out of
`build-main-runtime.sh` by the compatibility rule.

## Boundaries

- `T_NEXTN_HC_HEAD_*` ids (49–51) exist only in the integration tree, not in the
  pinned upstream; they are grouped separately in the corpus.
- The `else if (!ml.load_ngram)` skip-PLE branch exists only in the integration
  tree and is not modelled.
- HC gamma rank: canon `{n_embd, hc}` vs native `{hc_dim}`; element count and
  bytes match and the graph reshapes, but the difference is recorded, not
  resolved.
- The gate is structural (transcription oracle); no GGUF or weights are loaded.
- `q4m_recr_flag` returns `-1` for a non-positive interval instead of aborting;
  the integrator must treat it as fatal.
