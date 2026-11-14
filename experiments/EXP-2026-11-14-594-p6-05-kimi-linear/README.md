# kimi-linear builder slice (P6-05 architecture 11)

Native counterpart: `src/models/kimi-linear.cpp` (hybrid MLA / Kimi Delta
Attention MoE with a leading dense block). Base commit: `1925fc2a`. The
experiment id is assigned by the lead; this tree is the slice itself.

## What this slice covers

`source/kimi-linear.goose` is the plan/shape half of the builder, not the graph
emitter. It reuses the accepted common module `source/base/model_graph.goose`
(the canonical `goose/model/goose-model-graph.goose` without its `fn main`, byte
identical, SHA-256 `8c83ca66...`) for `key_dim`, `n_ff_exp`, `gating_kind`,
`kq_scale_auto` and `weights_need_scale`.

Decisions and shapes carried by the slice:

- **Per-layer kind** (`kl_recr_flags`, `kl_kind`, `kl_plan`). There is no
  hybrid interval in this architecture. The loader copies
  `n_head_kv(i) == 0` into `is_recr_impl[i]` (`:15-19`) and the graph only reads
  the resulting flag, so the whole plan follows from the per-layer KV-head
  array: `0` means KDA, anything else means MLA. There is no MTP block and the
  plan has no layer past `n_layer`.
- **What "GQA" means here.** `n_head_kv` is a *recurrent marker*, not a
  head-group width. On an MLA layer the KV side is one latent vector of
  `kv_lora_rank` values plus an unrotated `qk_rope_head_dim` slice, whatever
  `n_head_kv` says (`:126`); on a KDA layer q, k and v are all
  `n_head * n_embd_head_kda` wide because the loader sets the value head width
  equal to the key head width (`:54-55`, `:76`). No GQA view is ever built, and
  the slice deliberately carries no `key_dim`/`value_dim` case for the
  attention block.
- **MLA layer** (`:105-134`). Optional low-rank query: `attn_q_a_norm` present
  means `wq_a`/`wq_b`, absent means one `wq`; the latent key/value projection
  `wkv_a_mqa` is `[n_embd, kv_lora_rank + n_rot]`; the KV side has two
  load-time paths - a legacy combined `wkv_b` (`[kv_lora_rank,
  n_head*(nope + n_embd_head_v_mla)]`, MHA fallback) or the split
  `wk_b`/`wv_b` that the MLA KV cache absorbs; `wo` is
  `[n_head*n_embd_head_v_mla, n_embd]`. The key head split is
  `nope = n_embd_head_k_mla - n_rot` and **no rotary is applied**: `n_rot` is
  only a slice width (`:125`, `:273`, comment `:396`).
- **KDA layer** (`:53-103`, the Kimi Delta Attention form). Three separate
  causal conv1d weights of `{d_conv, 1, d_inner}` (`:59-73`), q/k/v projections
  of `[n_embd, d_inner]`, the decay pair `f_a`/`f_b` (`[n_embd, head_kda]`,
  `[head_kda, d_inner]`), one beta vector per head (`[n_embd, n_head]`),
  `A_log` of `{1, n_head}` (already negated at conversion), `dt_bias` over the
  whole inner width, the output-gate pair `g_a`/`g_b`, a per-head
  `o_norm` (FusedRMSNormGated) and `wo` of `[d_inner, n_embd]`.
- **KDA gate ops.** `beta` is a sigmoid and the output gate is a sigmoid
  applied to an RMS-normalised value (`:334`, `:376-379`), not the SiLU gate of
  the GDN families. `kl_kda_norm_before_gate` pins the order.
- **Recurrent state geometry.** `d_inner = n_head * head_kda`;
  `conv_state = (d_conv-1) * d_inner` per stream with three streams packed into
  one row (`llama-hparams.cpp:219-224`, `kimi-linear.cpp:177-178`);
  `kda_state = head_kda * head_kda * n_head` (`llama-hparams.cpp:242-247`).
- **Dense FFN and MoE** (`:140-164`, graph `:501-543`). Layers below
  `n_layer_dense_lead` carry `ffn_gate`/`ffn_down`/`ffn_up` of width `n_ff`;
  every other layer carries the router, the routed gate/down/up expert
  matrices of width `n_ff_exp` (per-layer array in the file) and a required
  per-layer `ffn_exp_probs_b` score bias. The shared expert is
  `n_ff_exp * max(n_expert_shared, 1)` wide - a multiple of the expert width,
  not `n_ff` as in the GDN families - and is added to the routed output.
- **Loader policy exports.** Required `output` (`:41`), no `attn_post_norm`
  (`:46`), no q/k RMS norm (`:112-113`), no rotary, `norm_w = true` for the
  router (`:520`), the file's gating function passed through unchanged, the
  MLA scale computed as `1/sqrt(n_embd_head_k_mla)` (`:276`), and the type
  table keyed on `n_layer == 27` (`:28-31`, unknown is not fatal).

## Gate

`tests/build.sh` builds the module with `--standalone` at `-O1`, links it with
`tests/fixture.cpp` and runs the structure cases:

```
FG-ARCH11 cases=103 values=195 failures=0
```

Cases and native anchors are listed in `tests/cases.md`. The gate checks the
plan and shapes only. It does not link a native library and does not execute a
graph: the expectations are a transcription of `kimi-linear.cpp` and the Kimi
branches of `llama-hparams.cpp`, so it proves the decisions and shapes, not
numeric parity.

## Mutations

`tests/mutate.py` holds 12 semantic mutations in two halves; each runs in its
own temporary tree at `-O0` while the gate stays at `-O1`.

```
python3 tests/mutate.py 0 6    # 6/6 caught
python3 tests/mutate.py 6 12   # 6/6 caught
```

Half 1 (plan and MLA): inverted recurrent marking, ignored recurrent flag,
dropped plan bounds, flipped no-RoPE decision, `nope` keeping the rotary prefix,
combined `wkv_b` losing the value width. Half 2 (KDA, FFN, policy): conv row
without the Q/K/V triplication, conv window counting `d_conv` taps, shared
expert without the count multiplier, dense-FFN boundary one layer too wide,
optional output, SiLU output gate.

## Recorded limits

- Structural gate only: no native library is linked and no graph is executed.
  Nothing here proves numeric parity with the native builder.
- `n_ff_exp` is a scalar, while the native loader reads a per-layer array
  (`n_ff_exp_arr[il]`, `:22`, `:140`). A checkpoint whose expert width changes
  per layer is outside the slice.
- Fixture geometry: every width the native file states in its own comments is
  used verbatim, while `n_vocab`, `n_expert`, `n_expert_used`, `n_head` and
  `ssm_d_conv` are fixture choices. The gate pins the formulas, not one
  particular checkpoint.
- `create_tensor_qkv` (`:76`) can also produce one fused `wqkv` (optionally
  with a bias) instead of three separate projections. The slice models the
  three-projection form only.
- The KDA value head width is assumed equal to the key head width, as the
  loader sets it (`:54-55`). A checkpoint with a different value head width is
  not covered.
- `A_log` is accepted in both file forms (`{1, n_head, 1, 1}` and
  `{1, n_head}`); the slice reports `{1, n_head, 1}` and does not distinguish
  the two.
- The conv weight is reported through its 3-D fallback form; the 4-D
  `TENSOR_NOT_REQUIRED` probe and its trailing unit dimension are not separately
  gated.
- The MLA graph's absorb path (query/view permutation, `kv_cmpr`/`k_pe` views,
  KV cache selection) and the KDA delta-net scan are graph construction and
  outside the slice. So are the softmax attention call and the final RMS norm.
- The type table is pinned by layer count only; an unknown type is reported as
  0 because native only changes the metadata name, not the shapes.
- `expert_gating_func` is a required key in native (`:26`), so an absent value
  is a load failure rather than a builder decision; the slice passes the value
  through and does not model the missing-key case.
- `build/` and `build-mut/` hold reproducible artifacts (generated C, objects,
  temporary mutation trees) and are not part of the evidence.
