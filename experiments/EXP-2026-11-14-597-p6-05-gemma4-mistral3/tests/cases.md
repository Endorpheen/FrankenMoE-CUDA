# Structure cases — gemma4, gemma4-assistant, mistral3 (P6-05 18/19/20)

Oracle: the native loader declarations in `src/models/gemma4.cpp`,
`src/models/gemma4-assistant.cpp`, `src/models/mistral3.cpp` and the per-layer
head helpers in `src/llama-hparams.cpp:115-140`. Values are transcribed from
those lines, not taken from a running library.

Three independent fixtures, three independent binaries, one gate line each:

```
FG-ARCH18 cases=36 values=143 failures=0
FG-ARCH19 cases=24 values=80  failures=0
FG-ARCH20 cases=36 values=183 failures=0
```

## Shared parameter layout

One 25-slot integer vector serves all three fixtures (`P_*` constants in the
Goose sources). Slots a family does not use are zero and are marked "unused"
below. Values are dimensions, not tensor data.

| slot | name | meaning |
|---:|---|---|
| 0 | `n_embd` | layer input width |
| 1 | `n_vocab` | vocabulary size |
| 2 | `n_layer` | effective layer count |
| 3 | `n_layer_all` | layer count including nextn blocks |
| 4 | `n_layer_nextn` | draft/nextn layer count |
| 5 | `n_head` | query heads |
| 6 | `n_head_kv` | key/value heads (GQA) |
| 7 | `n_embd_head_k` | full-attention key head width |
| 8 | `n_embd_head_v` | full-attention value head width |
| 9 | `n_embd_head_k_swa` | sliding-window key head width |
| 10 | `n_embd_head_v_swa` | sliding-window value head width |
| 11 | `n_rot` | rotated dimensions |
| 12 | `n_ff` | dense FFN length |
| 13 | `n_expert` | routed expert count (0 selects the dense branch) |
| 14 | `n_expert_used` | experts per token |
| 15 | `n_ff_exp` | declared expert FFN length |
| 16 | `n_ff_shexp` | declared shared-expert FFN length |
| 17 | `n_embd_per_layer` | PLE width, 0 disables PLE |
| 18 | `n_kv_shared_layers` | trailing layers that reuse an earlier KV |
| 19 | `n_embd_inp` | draft input (target hidden) width |
| 20 | `has_output` | separate output tensor present |
| 21 | `scale_bits` | `f_attention_scale` as float bits |
| 22 | `attn_temperature` | attention temperature scaling enabled |
| 23 | `longrope` | `rope_scaling_type_train == LONGROPE` |
| 24 | `soft_cap` | final logit softcapping enabled |

Two per-layer patterns are passed as raw `uint32_t` arrays, the same element
layout the base module uses for `is_recr_impl`: the sliding-window pattern
(`is_swa_impl`) and the loader's router-presence flags.

## Set A — gemma4 (architecture 18)

```
n_embd 2048, n_vocab 262144, n_layer 30 = n_layer_all, n_head 8, n_head_kv 2,
n_embd_head_k/v 256, n_embd_head_k/v_swa 128, n_rot 256, n_ff 16384,
n_expert 128, n_expert_used 8, n_ff_exp 1024, n_embd_per_layer 256,
n_kv_shared_layers 4, no separate output, f_attention_scale 1.0,
SWA pattern `il % 6 < 5`, router present from layer 4 on
```

Derived: full layer q width `256*8 = 2048`, k/v `256*2 = 512`; SWA layer
q `128*8 = 1024`, k/v `256`; PLE table rows `256*30 = 7680`; merged expert
gate_up `2*1024 = 2048`.

| # | case | expected | native anchor |
|---:|---|---|---|
| 1 | plan, 30 layers | `1,1,1,1,3,2` then `3*5,2` x4 | `gemma4.cpp:84`, `llama-hparams.cpp:264` |
| 2 | `token_embd.weight` | `{2048, 262144, 1}` | `:50` |
| 3 | `output.weight` | `{2048, 262144, 1}` | `:44-48` |
| 4 | `output_norm.weight` | `{2048, 1, 1}` | `:58` |
| 5 | `per_layer_token_embd.weight` | `{7680, 262144, 1}` | `:53` |
| 6 | `per_layer_model_proj.weight` | `{2048, 7680, 1}` | `:54` |
| 7 | `per_layer_proj_norm.weight` | `{256, 1, 1}` | `:55` |
| 8 | `attn_q.weight`, SWA layer 0 | `{2048, 1024, 1}` | `:73`, `:115-123` |
| 9 | `attn_k.weight`, SWA layer 0 | `{2048, 256, 1}` | `:74` |
| 10 | `attn_q.weight`, full layer 5 | `{2048, 2048, 1}` | `:73` |
| 11 | `attn_k.weight`, full layer 5 | `{2048, 512, 1}` | `:74` |
| 12 | `attn_v.weight`, full layer 5 | `{2048, 512, 1}` | `:75` |
| 13 | `attn_out.weight`, SWA layer 0 | `{1024, 2048, 1}` | `:76` |
| 14 | `attn_q_norm.weight`, SWA layer 0 | `{128, 1, 1}` | `:78` |
| 15 | `attn_q_norm.weight`, full layer 5 | `{256, 1, 1}` | `:78` |
| 16 | `attn_post_norm.weight` | `{2048, 1, 1}` | `:80` |
| 17 | `layer_out_scale.weight` | `{1, 1, 1}` | `:82` |
| 18 | `rope_freqs.weight`, full layer 5 | `{128, 1, 1}` | `:86` |
| 19 | `rope_freqs` on SWA layer 0 | absent | `:84-88` |
| 20 | `ffn_gate.weight`, dense layer 0 | `{2048, 16384, 1}` | `:95` |
| 21 | `ffn_down.weight`, dense layer 0 | `{16384, 2048, 1}` | `:97` |
| 22 | `ffn_post_norm.weight` | `{2048, 1, 1}` | `:98` |
| 23 | `ffn_gate_inp.weight` on dense layer 3 | absent | `:101-102` |
| 24 | `ffn_gate_inp.weight` on MoE layer 4 | `{2048, 128, 1}` | `:101` |
| 25 | `ffn_gate_inp.scale` (router norm) | `{2048, 1, 1}` | `:106` |
| 26 | `ffn_pre_norm_2.weight` | `{2048, 1, 1}` | `:108` |
| 27 | `ffn_post_norm_1.weight` | `{2048, 1, 1}` | `:109` |
| 28 | `ffn_post_norm_2.weight` | `{2048, 1, 1}` | `:110` |
| 29 | `ffn_gate_up_exps.weight` | `{2048, 2048, 128}` | `:113` |
| 30 | `ffn_gate_exps.weight` | `{2048, 1024, 128}` | `:116` |
| 31 | `ffn_up_exps.weight` | `{2048, 1024, 128}` | `:117` |
| 32 | `ffn_down_exps.weight` | `{1024, 2048, 128}` | `:120` |
| 33 | expert tensors on dense layer 3 | absent | `:105-123` |
| 34 | `per_layer_inp_gate.weight` | `{2048, 256, 1}` | `:127` |
| 35 | `per_layer_proj.weight` | `{256, 2048, 1}` | `:128` |
| 36 | `per_layer_post_norm.weight` | `{2048, 1, 1}` | `:129` |

Decisions checked as values (15): `has_kv` on the last owned and first shared
layer (25, 26); per-layer head width on an SWA and a full layer; `rope_freqs`
need `0/1/2`; router presence on a dense and a MoE layer; PLE active; output
optional; forced `kq_scale`; softcap off; last layer kind.

## Set B — gemma4-assistant (architecture 19)

```
n_embd 2048, n_vocab 262144, n_layer 6 = n_layer_all = n_layer_nextn,
n_head 8, n_embd_head_k/v 256, n_embd_head_k/v_swa 128, n_rot 256,
n_ff 8192, n_embd_inp 4096, no experts, no PLE,
SWA pattern `il % 6 < 5` (layer 0 SWA, layer 5 full)
```

| # | case | expected | native anchor |
|---:|---|---|---|
| 1 | plan, 6 nextn blocks | all `0` | `:50`, `:130` |
| 2 | `token_embd.weight` | `{2048, 262144, 1}` | `:37` |
| 3 | `output` (duplicated) | `{2048, 262144, 1}` | `:38` |
| 4 | `output_norm.weight` | `{2048, 1, 1}` | `:40` |
| 5 | masked-embd centroids | `{1, 1, 1}` | `:42` |
| 6 | masked-embd ordering | `{1, 1, 1}` | `:43` |
| 7 | `nextn_proj_post.weight` | `{2048, 4096, 1}` | `:46` |
| 8 | `nextn_proj_pre.weight`, layer 0 | `{8192, 2048, 1}` | `:58` |
| 9 | `nextn_proj_pre` on layer 1 | absent | `:57-59` |
| 10 | `attn_q.weight`, SWA layer 0 | `{2048, 1024, 1}` | `:62` |
| 11 | `attn_q.weight`, full layer 5 | `{2048, 2048, 1}` | `:62` |
| 12 | `attn_out.weight`, SWA layer 0 | `{1024, 2048, 1}` | `:63` |
| 13 | `attn_q_norm.weight`, SWA layer 0 | `{128, 1, 1}` | `:65` |
| 14 | `attn_q_norm.weight`, full layer 5 | `{256, 1, 1}` | `:65` |
| 15 | `attn_post_norm.weight` | `{2048, 1, 1}` | `:66` |
| 16 | `layer_out_scale.weight` | `{1, 1, 1}` | `:68` |
| 17 | `rope_freqs.weight`, full layer 5 | `{128, 1, 1}` | `:71` |
| 18 | `rope_freqs` on SWA layer 0 | absent | `:70-73` |
| 19 | `ffn_norm.weight` | `{2048, 1, 1}` | `:75` |
| 20 | `ffn_gate.weight` | `{2048, 8192, 1}` | `:76` |
| 21 | `ffn_up.weight` | `{2048, 8192, 1}` | `:77` |
| 22 | `ffn_down.weight` | `{8192, 2048, 1}` | `:78` |
| 23 | `ffn_post_norm.weight` | `{2048, 1, 1}` | `:79` |
| 24 | any tensor past the draft | absent | `:50`, `:130` |

Decisions checked as values (11): `n_layer_nextn == n_layer_all`; the two
hidden widths differ; no KV; `out_scale` required; forced `kq_scale`; per-layer
head width twice; `rope_freqs` need `0/1`; layer kind on the last layer and past
the draft.

## Set C — mistral3 (architecture 20)

```
n_embd 5120, n_vocab 131072, n_layer 40, n_head 40, n_head_kv 8,
n_embd_head_k/v 128, n_rot 128, n_ff 14336
set A: n_expert 128, n_expert_used 8, n_ff_shexp 2048, no output,
       f_attention_scale 0.0, no temperature, no longrope
set B: no experts, output present, f_attention_scale 1.0, longrope
```

| # | case | expected | native anchor |
|---:|---|---|---|
| 1 | plan, set A | 40 x `1` (MoE) | `:64`, `:172` |
| 2 | plan, set B | 40 x `0` (dense) | `:64-67` |
| 3 | `token_embd.weight` | `{5120, 131072, 1}` | `:32` |
| 4 | `output_norm.weight` | `{5120, 1, 1}` | `:35` |
| 5 | `output.weight` (duplicated) | `{5120, 131072, 1}` | `:36-41` |
| 6 | `attn_norm.weight` | `{5120, 1, 1}` | `:46` |
| 7 | fused `attn_qkv.weight` | `{5120, 7168, 1}` | `:48`, `llama-model.cpp:3146-3174` |
| 8 | `attn_q.weight` | `{5120, 5120, 1}` | `:48` |
| 9 | `attn_k.weight` | `{5120, 1024, 1}` | `:48` |
| 10 | `attn_v.weight` | `{5120, 1024, 1}` | `:48` |
| 11 | `attn_out.weight` | `{5120, 5120, 1}` | `:49` |
| 12 | `attn_out.bias` | `{5120, 1, 1}` | `:52` |
| 13 | `rope_freqs.weight`, set A | `{64, 1, 1}` | `:61` |
| 14 | longrope factors, set A | absent | `:56-62` |
| 15 | `rope_freqs` on a longrope file | absent | `:56-59` |
| 16 | `rope_factors_long.weight` | `{64, 1, 1}` | `:57` |
| 17 | `rope_factors_short.weight` | `{64, 1, 1}` | `:58` |
| 18 | `ffn_gate_inp.weight` | `{5120, 128, 1}` | `:74` |
| 19 | `ffn_gate_exps.weight` | `{5120, 14336, 128}` | `:75` |
| 20 | `ffn_down_exps.weight` | `{14336, 5120, 128}` | `:76` |
| 21 | `ffn_up_exps.weight` | `{5120, 14336, 128}` | `:77` |
| 22 | `ffn_gate_shexp.weight` | `{5120, 2048, 1}` | `:81` |
| 23 | `ffn_up_shexp.weight` | `{5120, 2048, 1}` | `:82` |
| 24 | `ffn_down_shexp.weight` | `{2048, 5120, 1}` | `:83` |
| 25 | dense `ffn_gate` on a MoE file | absent | `:73-85` |
| 26 | `ffn_norm.weight`, set B | `{5120, 1, 1}` | `:54` |
| 27 | `ffn_gate.weight`, set B | `{5120, 14336, 1}` | `:65` |
| 28 | `ffn_down.weight`, set B | `{14336, 5120, 1}` | `:66` |
| 29 | `ffn_up.weight`, set B | `{5120, 14336, 1}` | `:67` |
| 30 | `ffn_gate.bias` | `{14336, 1, 1}` | `:70` |
| 31 | `ffn_down.bias` | `{5120, 1, 1}` | `:71` |
| 32 | `ffn_up.bias` | `{14336, 1, 1}` | `:72` |
| 33 | router on a dense file | absent | `:73` |
| 34 | shared expert on a dense file | absent | `:80-84` |
| 35 | `attn_q.weight`, last layer | `{5120, 5120, 1}` | `:48` |
| 36 | any tensor past layer 39 | absent | `:43` |

Decisions checked as values (13): layer kind on both param sets and past the
last layer; `kq_scale` auto with `0.0f` and fixed with `1.0f`; fused QKV
optional; output optional; shared expert present/absent; temperature off/on;
rope tensor group for both files.

## Recorded limits

- Structural gate only: no native library is linked, no graph is executed, and
  no numeric result is compared. The gate proves the plan and the declared
  tensor shapes against a transcription of the native builders.
- The gemma4 per-layer router flags are the loader's tensor-presence scan. The
  fixture models a dense lead of four layers followed by MoE layers; the native
  code never derives this from hparams, so the fixture choice is only a way to
  exercise both kinds, not a claim about any released checkpoint.
- Two scalar tensors are approximated: the masked-embedding metadata of the
  assistant is declared `{}` (0-dim) natively and reported as `{1,1,1}` by the
  three-slot gate contract. The `layer_out_scale` tensors are `{1}` natively and
  are reported as `{1,1,1}` for the same reason.
- The `f_attention_scale` and `f_final_logit_softcapping` values enter as
  float bits (slot 21) or as a boolean (slot 24); the gate checks the decision,
  not the value.
- No case covers the assistant's `build_inp_per_layer`/`project_per_layer_inputs`
  path, gemma4's `cvec` control vectors (`build_cvec`), or any graph-level node
  order. Those are graph construction, outside this slice.
- Slots not listed per set are zero and unused; a family ignores them rather
  than validating them.
