# EXP-2026-11-14-586 — qwen3next builder (P6-05 architecture 5)

Native counterpart: `src/models/qwen3next.cpp` (hybrid gated delta net / full
attention MoE with an MTP head). Base commit: `5ab757e0`.

## What this slice covers

`source/qwen3next.goose` is the plan/shape half of the builder, not the graph
emitter. It reuses the accepted common module `source/base/model_graph.goose`
(the canonical `goose/model/goose-model-graph.goose` without its `fn main`) for
`is_recr`, `n_ff_exp`, `n_ff_shexp`, `key_dim`, `value_dim`, `conv_dim`,
`wqkv_width`, `head_v_dim`, `conv_channels`, `qkv_dim`, `q_full_width` and
`gating_kind`.

Decisions and shapes carried by the slice:

- **Per-layer kind** (`q3n_kind`, `q3n_plan`). Recurrent on every layer where
  `(il + 1) % full_attention_interval != 0`; full attention where the modulo is
  zero; the MTP kind on `[n_layer, n_layer_all)`. An explicit per-layer
  `is_recr_impl` array (`LLM_KV_ATTENTION_RECURRENT_LAYERS`) wins over the
  interval in both directions.
- **Full-attention layer.** Joint query+gate projection
  (`2 * n_embd_head_k * n_head`), GQA key/value blocks, `wo` of
  `n_embd_head_k * n_head` rows, and the mandatory q/k RMS norm of width
  `n_embd_head_k`.
- **Gated delta net layer.** Legacy `ssm_in` (`qkvz = 2*key + 2*value`), `wqkv`,
  `wqkv_gate`, `ssm_conv1d`, `ssm_dt`, `ssm_a`, `ssm_beta_alpha`
  (`2 * ssm_dt_rank`), `ssm_norm` and `ssm_out`, with the loader widths taken
  from `ssm_d_state`.
- **Routed and shared experts** on every kind, MTP included: the router
  projection, `n_ff_exp = n_ff / n_expert_used` (or the declared value), the
  gate/up/down expert matrices, a one-vector shared-expert gate and the shared
  expert widened to `n_ff` (or the declared `n_ff_shexp`).
- **MTP block.** The trunk tensor set plus `nextn.eh_proj` with the doubled
  hidden state, `enorm`, `hnorm`, the optional per-block `embed_tokens` and the
  optional `shared_head_head` / `shared_head_norm` override.
- **Loader policy exports**: `output` optional (duplicated from `token_embd`),
  q/k norm required, `attn_post_norm` on every layer, the MTP block count and
  the softmax routing kind.
- **GDN view geometry of the graph** (`q3n_gdn_graph_dims`): the native loader
  uses `head_v_dim = ssm_d_state`, while the graph recomputes
  `head_v_dim = ssm_d_inner / ssm_dt_rank`; the case pins that both agree for
  the fixture.

## Gate

`tests/build.sh` builds the module with `--standalone` at `-O1`, links it with
`tests/fixture.cpp` and runs the structure cases:

```
FG-ARCH5 cases=53 values=169 failures=0
```

Cases and native anchors are listed in `tests/cases.md`. The gate checks the
plan and shapes only. It does not link a native library: the expectations are a
transcription of `qwen3next.cpp`, so it proves the decisions and shapes, not
native numeric execution.

## Mutations

`tests/mutate.py` holds 12 semantic mutations in two halves; each runs in its
own temporary tree at `-O0` while the gate stays at `-O1`.

```
python3 tests/mutate.py 0 6    # 6/6 caught
python3 tests/mutate.py 6 12   # 6/6 caught
```

Half 1: hybrid interval, recurrent-flag array, MTP boundary, joint Q+gate width,
`wqkv` value width, q/k norm width. Half 2: legacy `ssm_in` qkvz width,
`ssm_norm` width, transposed token embedding, routed-expert width, shared-expert
fallback width, MTP `eh_proj` input width.

## Recorded limits

- Structural gate only: no native library is linked and no graph is executed.
  Nothing here proves numeric parity with the native builder.
- `n_ff_exp` and `n_ff_shexp` are scalars; native reads per-layer arrays
  (`n_ff_exp_arr[il]`, `n_ff(il)`), so a model whose expert width changes per
  layer is outside the slice.
- The optional fused QKV bias and the separate q/k/v tensors created when the
  fused weight is absent (`create_tensor_qkv`) are not in the shape table.
- An MTP layer that an explicit recurrent array marks as recurrent is not
  covered: every `il >= n_layer` is planned as `K_MTP`, whose tensor set is the
  attention block plus the nextn tensors, as `graph_mtp` builds it.
- The graph-vs-loader GDN divergence is pinned only for a fixture where
  `ssm_d_inner == ssm_d_state * ssm_dt_rank`; a model where they differ is not
  covered.
- `interval <= 0` maps to all-attention; the native modulo by zero is
  undefined and not reproduced.
- The attention gate split, the GDN l2 norm/repeat/state path, the
  shared-expert sigmoid and the post-norm residual wiring are graph
  construction and outside this slice.
- `build/` and `build-mut/` hold reproducible artifacts (generated C, objects,
  temporary mutation trees) and are not part of the evidence.
