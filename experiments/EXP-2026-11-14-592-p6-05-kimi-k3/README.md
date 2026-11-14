# kimi-k3 builder (P6-05 architecture 10)

Native counterpart: `src/models/kimi-k3.cpp` (Kimi-K3 2.8T-A50B: hybrid KDA +
MLA attention, latent MoE, cross-layer residual attention, situ activation).
Base commit: `766c2344`.

## What this slice covers

`source/kimi-k3.goose` is the plan/shape half of the builder, not the graph
emitter. It owns, decision by decision:

- **plan** — `mk_layer_kind` / `mk_plan`: one FFN kind per layer from
  `n_layer_dense_lead` (dense lead) and the trunk length; `K_MTP` exists in the
  kind set but this family never plans it. `mk_plan_ok` refuses a file whose
  declared block count exceeds the trunk instead of silently dropping blocks.
- **attention marker** — `mk_is_recr` / `mk_attn_kind`: `n_head_kv == 0`
  selects KDA (recurrent linear attention), any positive count selects MLA
  (`kimi-k3.cpp:31-33`). The value is a boolean marker, not a grouped-query
  divisor: MLA keeps one latent KV row per token (`mk_kv_rows`), and
  `mk_gqa_ok` restricts a positive marker to one that divides `n_head`.
- **kda** — the conv windows (`{d_conv, 1, d_inner}`), the fused QKV width
  `3*d_inner`, the decay pair `f_a`/`f_b`, `beta`, the plain 1-D `A_log`, the
  `dt` bias, the single full-rank gate `ssm_g`, `o_norm` and `wo`; the
  lower-bound gate swap (`mk_kda_gate_low`) is a decision, the magnitude is a
  float consumed by the graph.
- **mla** — the low-rank query path (`wq_a` → `q_a_norm` → `wq_b`) or the plain
  `wq` when no lora rank is declared; the joint latent projection
  `wkv_a_mqa {n_embd, kv_lora + n_rot}`, `kv_a_norm`, the unabsorbed `wkv_b`
  and the absorbed `wk_b`/`wv_b` alternative, the sigmoid output gate
  `{n_embd, n_head*n_embd_head_v}`, and `wo`. `mk_nope` keeps the nope-only
  head split (`n_embd_head_k - n_rot`).
- **latent-moe** — `mk_embd_latent` (the latent width or `n_embd`), routed
  experts with the latent as their input row, `routed_down`/`routed_up`/
  `routed_norm` only when a latent length is declared, and the router left on
  the full-width input.
- **shexp** — `mk_shexp_width`: `n_ff_exp * max(n_expert_shared, 1)`; the three
  tensors stay optional in the file.
- **res-score** — `mk_has_res`, `mk_res_push` (`il % attn_res_block_size == 0`),
  `mk_res_points`, `mk_res_scores_width` (`n_ckpt + 1`) and
  `mk_res_mix_active` (the mix is a no-op while no checkpoint is banked). The
  score vectors are `{n_embd}` on every layer plus the model-level output
  score.
- **nextn** — this native has none. `mk_mtp_supported()` returns 0: the loader
  and the graph iterate `n_layer` only, create no `nextn.*` tensors and run no
  MTP pass, so `mk_plan_ok` rejects a declared nextn block rather than
  pretending to plan one.
- **gating/activation** — `mk_gating_ok` (the four gating functions
  `build_moe_ffn` implements), `mk_situ_up` (`situ_linear_beta > 0` on the bit
  pattern), and the shared `n_ff_exp` fallback from `base.model_graph`.

`source/base/model_graph.goose` is the accepted shared slice without its
`fn main` (canonical file `goose/model/goose-model-graph.goose`); the builder
imports it for `n_ff_exp`.

## Gate

`tests/build.sh` builds both modules with `--standalone` at `-O1` and runs 126
structure cases:

```
FG-ARCH10 cases=126 values=226 failures=0
```

Cases and parameters are listed in `tests/cases.md`. The gate checks the plan
and shapes only. It does not link a native library: the expectations are a
transcription of `kimi-k3.cpp`, so it proves the decisions and shapes, not
native numeric execution.

## Mutations

`tests/mutate.py 0 6|6 12` holds 12 semantic mutations in two halves; each runs
in its own temporary tree at `-O0` while the gate stays at `-O1`.

```
python3 tests/mutate.py 0 6    # 6/6 caught
python3 tests/mutate.py 6 12   # 6/6 caught
```

Mutations cover the dense-lead boundary, the MTP refusal, the KDA/MLA marker,
the marker divisibility rule, the residual checkpoint cadence and count, the
latent width, the nope share, the joint latent projection width, the
routed-expert input width, the MLA query width and the shared-expert width.

## Recorded limits

- **Structural only.** The gate compares the plan and the shape table against
  a hand transcription of the native loader. No native library is linked, no
  GGUF is loaded, and the graph is never executed, so no numeric claim is made.
- **Synthetic fixture.** The parameter vector, the per-layer expert-width array
  and the `n_head_kv` marker array are a plausible Kimi-K3-sized configuration,
  not a real checkpoint header. The gate is sensitive to the rules, not to
  that particular file.
- **No native MTP.** Kimi-K3 has no nextn block in this tree; the slice models
  that refusal explicitly instead of copying another family's MTP shapes.
- **`n_ff_exp` fallback is ours.** The native loader requires the per-layer
  expert-width array; the slice falls back to `n_ff / n_expert_used` through
  the shared helper when an entry is zero, so a zero entry is a decision here
  and an error natively.
- **`mk_gqa_ok` is a sanity rule, not a native assert.** The native only tests
  `n_head_kv == 0`; the divisibility rule is a builder guard for a positive
  marker.
- **Abort paths are not exercised.** `mk_plan`'s MTP `fail` and `mk_nope`'s
  `n_rot > head_k` guard have no negative case.
- **State is out of scope.** `n_embd_r`/`n_embd_s`, the conv-state rollback,
  the recurrent scan and the MLA latent cache belong to the KV/state
  subsystem, not to this builder slice.
