# glm5-next builder (P6-05 architecture 12)

Native counterpart: `src/models/glm5-next.cpp` (GLM-5.3-Flash family: hybrid
KDA gated-delta-net and nope-MLA layers, a k-pool DSA indexer, mHC
hyper-connections, dense-lead plus routed/shared experts, one NextN block).
Base commit: `766c23441516ad9ebaae6b355085252fe1092c4d`.

## What this slice covers

`source/glm5-next.goose` is the plan/shape half of the builder, not the graph
emitter. It owns, family by family:

- **plan** — `g5_attn_kind` / `g5_layer_kind` / `g5_plan`: a trunk layer with
  zero KV heads is a KDA layer, any other trunk layer is nope MLA; layers at
  and past `n_layer` are the MTP block, marked before the KV rule because the
  MTP graph always runs the DSA branch (`:22`, `:548-616`).
- **ffn** — `g5_ffn_kind`: the leading `n_layer_dense_lead` layers build one
  dense SwiGLU FFN, every later trunk layer and the MTP block build routed
  experts plus shared experts (`:155-173`, `:630-649`).
- **mla** — low-rank query (`wq_a` → `attn_q_a_norm` → `wq_b`), the compressed
  KV (`wkv_a_mqa` → `attn_kv_a_norm`), the absorbed `wk_b`, `wv_b` and the
  output projection; `g5_mla_nope_ok` holds the no-rope assert (`:1046`), and
  `g5_mla_cache_width` records that the KV cache stores the latent.
- **kda (gdn)** — `d_inner = n_embd_head_kda * n_head`, the three causal
  convolutions, the decay `f_a`/`f_b`, `ssm_a`, `ssm_dt_b`, the output gate
  `g_a`/`g_b`, the output norm and `wo`; `g5_kda_gate_kind` records whether a
  finite `kda_gate_lower_bound` selects the sigmoid-bounded decay path or the
  softplus path (`:834-846`), and `g5_kda_conv_state` the conv state width.
- **indexer / dsa** — `g5_indexer_full` (trunk flag array, MTP always full),
  `g5_has_indexer` (MLA only), the k-pool legality rule `kpool > 1` and
  `top_k % kpool == 0`, the top-pool cap `min(n_pool, top_k/kpool)`, the
  selection width `kpool*n_top_pool + (tail ? kpool-1 : 0)` and the sparse mask
  width `n_kv + n_sel`. A shared (non-full) MLA layer reuses the previous full
  layer's selection (`:141-152`, `:322-329`, `:1002`, `:1069-1073`).
- **hc** — hyper-connection multiplicity, `hc_mix_dim = (2+hc)*hc`, the mixer
  input `hc*n_embd`, the `hc == 4` restriction, both per-layer modules (attn and
  ffn) on trunk layers only, and the flat MTP output width. Unlike deepseek4 the
  MTP hidden state is `n_embd` wide: the trunk averages the streams before the
  head, so the assert is `n_embd_out() == n_embd` (`:53`, `:65`, `:85-90`,
  `:461`, `:564`, `:777-784`).
- **moe / shexp** — per-layer routed width `n_ff_exp_arr[il]` (no fallback),
  router, score bias, the three expert matrices, and one shared expert of
  `n_ff_exp * n_expert_shared` width added to the routed output. Gating
  defaults to SIGMOID when the key is absent, and `swiglu_clamp_shexp` falls
  back to `swiglu_clamp_exp` (`:31-38`, `:160-173`).
- **nextn** — the MTP probe, the single-block restriction, the doubled
  `eh_proj` input `2*n_embd`, `enorm`/`hnorm`, and the optional own embedding
  and shared head (`:175-183`, `:548-555`).

`source/base/model_graph.goose` is the accepted shared slice without its
`fn main` (canonical file `goose/model/base/model_graph.goose`); the builder
imports it.

## Gate

`tests/build.sh` builds both modules with `--standalone` at `-O1` and runs 153
structure cases:

```
FG-ARCH12 cases=153 values=287 failures=0
```

Cases and parameters are listed in `tests/cases.md`. The gate checks the plan
and shapes only. It does not link a native library: the expectations are a
transcription of `glm5-next.cpp`, so it proves the decisions and shapes, not
native numeric execution.

## Mutations

`tests/mutate.py 0 6|6 12` holds 12 semantic mutations in two halves; each runs
in its own temporary tree at `-O0` while the gate stays at `-O1`.

```
python3 tests/mutate.py 0 6    # 6/6 caught
python3 tests/mutate.py 6 12   # 6/6 caught
```

Mutations cover the KV-head plan rule, the MTP plan boundary, the dense-lead
boundary, the KDA inner width, the HC mixer width, the shared-expert width, the
MLA query and output widths, the `wk_b` orientation, the k-pool tail decision,
the top-pool division and the MTP input width.

## Recorded limits

- **Structural only.** The gate compares the plan and the shape table against a
  hand transcription of the native loader. No native library is linked, no GGUF
  is loaded, and the graph is never executed, so no numeric claim is made.
- **Synthetic fixture.** The parameter vector, the KV-head array, the per-layer
  expert widths and the indexer flags are a plausible GLM-5.3-Flash-sized
  configuration, not a real checkpoint header. The gate is sensitive to the
  rules, not to that particular file.
- **KV-head resolution is simplified.** The fixture passes `n_head_kv(i)`
  directly; the native `is_recr` array is filled after SWA/variant head
  resolution, which is not modelled here.
- **Shared indexer tensors.** Native still creates optional copies of the
  indexer tensors on a non-full layer (`TENSOR_NOT_REQUIRED`). The gate reports
  them as not owned by the plan because no selection graph is built; it does not
  test whether an optional copy is present in a file.
- **Abort paths are not exercised.** Short arrays and the out-of-range `e_ensure`
  guards call `fail`, which the fixture records but does not provoke. Only the
  decision functions have negative cases (`kpool_ok`, `mla_nope_ok`,
  `hc_supported`, `nextn_single`).
- **KDA recurrent state is out of scope.** The conv-state ring (`n_embd_r`) and
  the SSM state (`n_embd_s`) belong to the recurrent/KV subsystem, not to this
  builder slice. `g5_kda_conv_state` is recorded as a decision only.
- **The decay bound is a flag.** `kda_gate_lower_bound` is a float; the gate
  checks only which branch a finite value selects, not the magnitude.
- **No attention sinks.** This architecture has none, which is why the deepseek4
  sink cases have no counterpart here.
