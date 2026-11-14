# qwen3next structure cases (P6-05 architecture 5)

Oracle: the native loader declarations and the native graph view geometry in
`src/models/qwen3next.cpp`. The values are read from those lines; no native
library is linked.

The fixture uses one 20-element parameter layout (the `P_*` constants in
`source/qwen3next.goose` and here):

```
0  n_embd                  7  n_ff                 14 ssm_d_state
1  n_vocab                 8  n_expert             15 ssm_dt_rank
2  n_layer                 9  n_expert_used        16 ssm_n_group
3  n_layer_all            10  n_ff_exp declared    17 full_attention_interval
4  n_head                 11  n_ff_shexp declared  18 has_output
5  n_head_kv              12  ssm_d_conv           19 expert gating func
6  n_embd_head_k          13  ssm_d_inner
```

Fixture values: `n_embd 2048, n_vocab 151936, n_layer 48, n_layer_all 49,
n_head 16, n_head_kv 2, head 128, n_ff 8192, n_expert 512, used 10,
n_ff_exp declared 0, n_ff_shexp declared 0, ssm_d_conv 4, ssm_d_inner 4096,
ssm_d_state 128, ssm_dt_rank 32, ssm_n_group 16, interval 4, output optional,
gating softmax`.

Derived: `n_ff_exp = 8192/10 = 819`, `n_ff_shexp = 8192`,
`n_embd_k_gqa = n_embd_v_gqa = 128*2 = 256`, joint Q+gate width
`= 2*128*16 = 4096`, fused QKV `= 4096+256+256 = 4608`, `wo = 128*16 = 2048`,
GDN `key = 128*16 = 2048`, `value = 128*32 = 4096`, `conv = 8192`,
`qkvz = 12288`, `ba = 64`.

## Plan and kind (cases 1-10)

| # | case | expected | native anchor |
|---|---|---|---|
| 1 | plan, 49 blocks | recurrent on `il % 4 != 3`, attention on `il % 4 == 3` inside 0..47, MTP on 48 | `qwen3next.cpp:17-23`, `:126-128` |
| 2 | `kind(0)` | 1 (recurrent) | `:21` |
| 3 | `kind(3)` | 0 (attention) | `:21` |
| 4 | `kind(46)` | 1 (recurrent) | `:21` |
| 5 | `kind(47)` | 0 (attention) | `:21` |
| 6 | `kind(48)` | 2 (MTP) | `:110`, `:126-128`, `:618` |
| 7 | `kind(-1)` | -1 | `:163` (fatal below zero) |
| 8 | `kind(49)` | -1 | `:163` (fatal past `n_layer_all`) |
| 9 | `kind(0, flags on)`, flag 0 = 0 | 0 (attention; the interval says recurrent) | `:17`, `:163` |
| 10 | `kind(3, flags on)`, flag 3 = 1 | 1 (recurrent; the interval says attention) | `:17`, `:163` |

Cases 9-10 make the per-layer `is_recr_impl` array beat the interval in both
directions, which needs a real 4-byte-per-layer array in the `ld_u32` shim.

## Loader policy (cases 11-16)

| # | case | expected | native anchor |
|---|---|---|---|
| 11 | output is optional, duplicated from `token_embd` | 1 | `:46-51` |
| 12 | q/k RMS norm required on attention layers | 1 | `:80-81` |
| 13 | `attn_post_norm` on every layer | 1 | `:73`, `:749` |
| 14 | MTP block count `n_layer_all - n_layer` | 1 | `:126-128` |
| 15 | routed gating kind | 0 (softmax) | `:570` |
| 16 | GDN graph view dims | `{128, 8192, 8192}` | `:398-404`, `:457`, `:474` |

## Global tensors and layer norms (cases 17-21)

| # | tensor | expected | native anchor |
|---|---|---|---|
| 17 | `token_embd.weight` | `{2048, 151936, 1}` | `:42` |
| 18 | `output_norm.weight` | `{2048, 1, 1}` | `:45` |
| 19 | `output.weight` | `{2048, 151936, 1}` | `:46` |
| 20 | attention `attn_norm.weight` | `{2048, 1, 1}` | `:72` |
| 21 | recurrent `attn_post_norm.weight` | `{2048, 1, 1}` | `:73` |

## Full-attention layer (cases 22-25)

| # | tensor | expected | native anchor |
|---|---|---|---|
| 22 | fused `attn_qkv.weight` (joint Q+gate, GQA K/V) | `{2048, 4608, 1}` | `:77`, `llama-model.cpp:3860-3863` |
| 23 | `attn_wo.weight` | `{2048, 2048, 1}` | `:78` |
| 24 | `attn_q_norm.weight` | `{128, 1, 1}` | `:80` |
| 25 | `attn_k_norm.weight` | `{128, 1, 1}` | `:81` |

## Gated delta net layer (cases 26-35)

| # | tensor | expected | native anchor |
|---|---|---|---|
| 26 | recurrent `attn_norm.weight` (second kind) | `{2048, 1, 1}` | `:72` |
| 27 | `ssm_in.weight` (legacy qkvz) | `{2048, 12288, 1}` | `:86`, `:65` |
| 28 | `attn_qkv.weight` (wqkv) | `{2048, 8192, 1}` | `:87` |
| 29 | `attn_gate.weight` (wqkv_gate) | `{2048, 4096, 1}` | `:88` |
| 30 | `ssm_conv1d.weight` | `{4, 8192, 1}` | `:89` |
| 31 | `ssm_dt.bias` | `{32, 1, 1}` | `:90` |
| 32 | `ssm_a` | `{32, 1, 1}` | `:91` |
| 33 | `ssm_beta_alpha.weight` | `{2048, 64, 1}` | `:92`, `:66` |
| 34 | `ssm_norm.weight` | `{128, 1, 1}` | `:93` |
| 35 | `ssm_out.weight` | `{4096, 2048, 1}` | `:94` |

## Routed and shared experts (cases 36-43)

| # | tensor | expected | native anchor |
|---|---|---|---|
| 36 | `ffn_gate_inp.weight` | `{2048, 512, 1}` | `:97` |
| 37 | `ffn_down_exps.weight` | `{819, 2048, 512}` | `:98` |
| 38 | `ffn_gate_up_exps.weight` | `{2048, 1638, 512}` | `:99` |
| 39 | `ffn_gate_exps.weight` | `{2048, 819, 512}` | `:99` |
| 40 | `ffn_up_exps.weight` (on a recurrent layer) | `{2048, 819, 512}` | `:99` |
| 41 | `ffn_gate_inp_shexp.weight` (single vector) | `{2048, 1, 1}` | `:102` |
| 42 | `ffn_gate_shexp.weight` | `{2048, 8192, 1}` | `:103` |
| 43 | `ffn_down_shexp.weight` (on a recurrent layer) | `{8192, 2048, 1}` | `:105` |

## MTP block (cases 44-49)

| # | tensor | expected | native anchor |
|---|---|---|---|
| 44 | `nextn.eh_proj.weight` | `{4096, 2048, 1}` | `:115` |
| 45 | `nextn.enorm.weight` | `{2048, 1, 1}` | `:116` |
| 46 | `nextn.hnorm.weight` | `{2048, 1, 1}` | `:117` |
| 47 | `nextn.embed_tokens.weight` | `{2048, 151936, 1}` | `:118` |
| 48 | `nextn.shared_head_head.weight` | `{2048, 151936, 1}` | `:119` |
| 49 | `nextn.shared_head_norm.weight` | `{2048, 1, 1}` | `:120` |

## Kind separation (cases 50-53)

| # | case | expected |
|---|---|---|
| 50 | `wqkv` requested for an attention layer | absent (0) |
| 51 | fused `attn_qkv` requested for a recurrent layer | absent (0) |
| 52 | `attn_q_norm` requested for a recurrent layer | absent (0) |
| 53 | `nextn.eh_proj` requested for an attention layer | absent (0) |

Expected: `FG-ARCH5 cases=53 values=169 failures=0` (49 plan kinds + 7 kind
values + 2 flag values + 5 policy values + 3 graph dims + 33 shapes x 3 + 4
absence values = 169).

## Recorded limits

- The gate is structural: it checks plan and shapes against a transcription of
  the native loader and graph view geometry. It does not link the native library
  and does not execute a graph.
- `n_ff_exp` and `n_ff_shexp` are scalars here. Native reads per-layer arrays
  (`n_ff_exp_arr[il]`, `n_ff(il)`); the fixture declares one value for all
  layers.
- `load_block_trunk` also creates an optional fused QKV bias
  (`create_tensor_qkv`, `llama-model.cpp:3877-3883`) and separate q/k/v tensors
  when the fused weight is absent. Those are not in the shape table.
- The legacy recurrent path (`ssm_in` present, `wqkv` absent) is a graph branch
  decision, not a shape decision; only the tensor widths are pinned.
- An MTP layer that an explicit recurrent array marks as recurrent
  (`load_block_mtp` -> `load_block_trunk`) is not covered: the plan assigns
  every `il >= n_layer` to `K_MTP`, whose tensor set is the attention block plus
  the nextn tensors, as `graph_mtp` builds it.
- The GDN case pins that the loader geometry (`head_v_dim = ssm_d_state`) and
  the graph geometry (`head_v_dim = ssm_d_inner / ssm_dt_rank`) agree for this
  fixture (128 / 8192 / 8192). A model where `ssm_d_inner != ssm_d_state *
  ssm_dt_rank` would diverge and is not covered.
- `interval <= 0` returns all-attention; native would take a modulo by zero.
- The shared-expert sigmoid gate, the attention gate split and the post-norm
  residual wiring are graph construction and outside this slice.
