# EXP-2026-11-14-586 — deepseek32 builder (P6-05 architecture 8)

Native counterpart: `src/models/deepseek32.cpp` (DeepSeek V3.2 family: MLA
attention, DSA lightning indexer, dense lead + routed/shared-expert MoE, NextN
MTP draft head). Base commit: `5ab757e0`.

## What this slice covers

`source/deepseek32.goose` is the plan/shape half of the builder, not the graph
emitter. It owns:

- the per-layer plan: the dense-lead/MoE switch at `n_layer_dense_lead`, the
  trunk/MTP switch at `n_layer`, and the two crossed kinds
  (`K_DENSE`, `K_MOE`, `K_MTP_DENSE`, `K_MTP_MOE`);
- the MLA shapes: `wq_a {n_embd,q_lora_rank}`, `wq_b
  {q_lora_rank,n_head*head_k_mla}`, `wkv_a_mqa
  {n_embd,kv_lora_rank+n_rot}`, the split `wk_b {nope,kv_lora,n_head}` and
  `wv_b {kv_lora,head_v,n_head}`, and `wo {n_head*head_v,n_embd}`;
- the per-rank q/k normalisation: `attn_q_a_norm {q_lora_rank}` and
  `attn_kv_a_norm {kv_lora_rank}`, not per-head norms;
- the DSA indexer decisions: five indexer tensors on every loader layer, the
  `min(n_cached, indexer_top_k)` clamp, the `is_mla` gate (both MLA head sizes
  set or neither), and the fact that the MTP draft graph does not execute the
  indexer;
- the routed-expert width (`n_ff_exp`, declared or `n_ff/n_expert_used`) and the
  shared-expert width `n_ff_exp * n_expert_shared`;
- the NextN/MTP tensors: `eh_proj {2*n_embd,n_embd}`, `enorm`/`hnorm`,
  optional `embed_tokens`/`shared_head_head`/`shared_head_norm`.

`source/base/model_graph.goose` is the accepted model-graph slice at commit
`5ab757e0` (the canonical file without its `fn main()`, 367 lines). This slice
reuses `n_ff_exp` and `gating_kind` from it.

## Gate

`tests/build.sh` builds both modules with `--standalone` at `-O1` and runs 54
structure cases (130 checked values):

```
FG-ARCH8 cases=54 values=130 failures=0
```

The cases are transcribed in `tests/cases.md`; the expected line has the
`FG-ARCH8` prefix and `failures=0`.

## Mutations

`tests/mutate.py 0 6|6 12` holds 12 semantic mutations in two halves; each runs
in its own temporary tree at `-O0` while the gate stays at `-O1`.

```
python3 tests/mutate.py 0 6    # 6/6 caught
python3 tests/mutate.py 6 12   # 6/6 caught
```

Mutations cover the dense-lead boundary, MTP layer planning, the `wq_b` head
width, the rope part of `wkv_a_mqa`, the `wk_b` axes, the indexer q projection,
the shared-expert multiplier, the routed-expert width, the doubled MTP hidden
state, the token-embedding orientation, indexer use on MTP, and the top-k clamp.

## Recorded limits

- The gate is structural: it checks plan and shapes against a transcription of
  the native builder. It does not link the native library and does not execute
  the graph.
- The fixture uses DeepSeek V3.2-scale parameter values but is a shape fixture,
  not a real GGUF. `n_expert_shared` is 2 (released: 1) so the shared-expert
  multiplier is exercised; `n_ff_exp` is the declared value.
- The MLA absorption (`q_nope_absorbed`, `Qcur`/`Kcur` concatenation), rope
  scaling (`kq_scale`, YaRN factor), the DSA Hadamard transform and the indexer
  scoring graph are not represented; only their shape inputs and the top-k
  decision are.
- `n_layer_nextn` is a parameter, but the native MTP graph requires exactly one
  block (`deepseek32.cpp:489-490`). The plan handles one or more MTP layers;
  the gate exercises one.
- Many one-value decisions are exported without negative cases: the half-set
  MLA abort, the zero-expert abort, and the gating types other than softmax.
