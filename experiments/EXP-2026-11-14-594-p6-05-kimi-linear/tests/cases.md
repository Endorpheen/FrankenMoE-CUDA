# kimi-linear structure cases (P6-05 architecture 11)

Oracle: the native loader declarations and the native graph geometry in
`src/models/kimi-linear.cpp`, plus the Kimi KDA branches of
`src/llama-hparams.cpp` (`n_embd_r`, `n_embd_s`, `is_recr`). The values are read
from those lines; no native library is linked.

The fixture uses one 21-element parameter layout (the `P_*` constants in
`source/kimi-linear.goose` and here):

```
0  n_embd              7  n_ff_exp declared   14 q_lora_rank
1  n_vocab             8  n_expert_shared     15 qk_rope_head_dim (n_rot)
2  n_layer             9  ssm_d_conv          16 n_layer_dense_lead
3  n_head             10  n_embd_head_kda     17 attn_q_a_norm present
4  n_ff                11 n_embd_head_k_mla   18 combined wkv_b present
5  n_expert            12 n_embd_head_v_mla   19 expert_gating_func
6  n_expert_used       13 kv_lora_rank        20 expert_weights_scale bits
```

Default fixture (48B-A3B geometry, modern layout): `n_embd 2304,
n_vocab 163840, n_layer 27, n_head 32, n_ff 9216, n_expert 256, used 8,
n_ff_exp 1024, shared 1, d_conv 4, head_kda 128, head_k_mla 192,
head_v_mla 128, kv_lora 512, q_lora 0 (absent), n_rot 64, dense_lead 1,
no q lora, split `wk_b`/`wv_b`, softmax routing, score scale 2.446f
(`0x401C8B44` = 1075628868)`.

The widths the native file states in its own comments are used verbatim
(`n_embd 2304` and `wkv_a_mqa 576` at `:272`, `d_inner 4096` at `:265`,
`n_ff 9216` and `n_ff_exp 1024` at `:150`, `kda_state 524288` at
`llama-hparams.cpp:246`, 27 layers at `:29`). `n_vocab`, `n_expert`,
`n_expert_used`, `n_head` and `ssm_d_conv` are fixture choices: the native file
does not state them, and no case depends on a particular checkpoint.


Legacy / compressed variant: `n_expert_shared 2, q_lora_rank 1024,
dense_lead 2, attn_q_a_norm present, combined wkv_b present`.

`n_head_kv` per layer: `0` on every layer but `il % 4 == 3`, where it is
`n_head`. The plan therefore has 6 MLA layers (3, 7, 11, 15, 19, 23) and 21 KDA
layers.

Derived: `d_inner = 128*32 = 4096`, `conv_state = 3*4096 = 12288`,
`conv_total = 36864`, `kda_state = 128*128*32 = 524288`, `nope = 192-64 = 128`,
`wkv_b width = 32*(128+128) = 8192`, `q width = 32*192 = 6144`,
`wo MLA = 32*128 = 4096`, `shared expert = 1024*1 = 1024` (2048 in the
variant).

## Per-layer kind and plan (cases 1-12)

| # | case | expected | native anchor |
|---|---|---|---|
| 1 | `kl_recr_flags` returns 27 | 27 | `:17-19` |
| 2 | flag of a KDA layer (0) | 1 | `:18`, `n_head_kv(i) == 0` |
| 3 | flag of an MLA layer (3) | 0 | `:18` |
| 4 | flag of the last layer (26) | 1 | `:18` |
| 5 | `kl_plan` length | 27 | `:17` |
| 6 | `kl_plan` kinds differ from the flag array nowhere | 0 mismatches | `llama-hparams.cpp:260-265` |
| 7 | `kl_kind(0)` | 1 (KDA) | `:18` |
| 8 | `kl_kind(3)` | 0 (MLA) | `:18` |
| 9 | `kl_kind(23)` | 0 (MLA) | `:18` |
| 10 | `kl_kind(26)` | 1 (KDA) | `:18` |
| 11 | `kl_kind(-1)` | -1 | `llama-hparams.cpp:263-265` (abort) |
| 12 | `kl_kind(27)` | -1 | `llama-hparams.cpp:263-265` |

Cases 1-12 pin the decisive difference from the GDN families: this architecture
has **no hybrid interval**. The loader copies `n_head_kv == 0` into
`is_recr_impl` and the graph only reads that flag.

## Loader policy (cases 13-24)

| # | case | expected | native anchor |
|---|---|---|---|
| 13 | `output` is required | 1 | `:41` (flags 0) |
| 14 | no `attn_post_norm` | 0 | `:46` (only `attn_norm`) |
| 15 | no q/k RMS norm | 0 | `:112-113` (only `kv_a_norm`) |
| 16 | no rotary at all | 0 | `:125`, `:273`, comment `:396` |
| 17 | MoE renormalises routed scores | 1 | `:520` (`norm_w = true`) |
| 18 | KDA beta is sigmoid | 2 | `:334` |
| 19 | KDA output gate is sigmoid | 2 | `:378` |
| 20 | the gate multiplies a normalised value | 1 | `:376-379` |
| 21 | routing kind, softmax in the file | 0 | `:522` (pass-through) |
| 22 | routing kind, sigmoid in the file | 1 | `:522` |
| 23 | score scale 2.446f is applied | 1 | `:521`, `:25` |
| 24 | MLA scale is computed, not read | 1 | `:276` |

## Layout switches (cases 25-30)

| # | case | expected | native anchor |
|---|---|---|---|
| 25 | type known for 27 layers | 1 | `:28-29` |
| 26 | type unknown for 28 layers | 0 | `:30` |
| 27 | q lora absent in the default file | 0 | `:112`, `:118-120` |
| 28 | split `wk_b`/`wv_b` in the default file | 1 | `:127-132` |
| 29 | q lora present in the variant | 1 | `:115-117` |
| 30 | combined `wkv_b` in the variant | 0 | `:128-130` |

## Derived geometry (cases 31-39)

| # | case | expected | native anchor |
|---|---|---|---|
| 31 | `d_inner` | 4096 | `:265` |
| 32 | `conv_state` | 12288 | `llama-hparams.cpp:219-224` |
| 33 | `conv_total` (Q+K+V) | 36864 | `:177-178` |
| 34 | `kda_state` | 524288 | `llama-hparams.cpp:242-247` |
| 35 | `nope` | 128 | `:274` |
| 36 | `wkv_b` width | 8192 | `:129` |
| 37 | shared expert, 1 expert | 1024 | `:159` |
| 38 | shared expert, 2 experts | 2048 | `:159` |
| 39 | shared expert, 0 experts | 1024 | `:159` (fallback 1) |

## FFN branch boundary (cases 40-45)

| # | case | expected | native anchor |
|---|---|---|---|
| 40 | layer 0 is dense | 0 | `:144` |
| 41 | layer 1 is MoE | 1 | `:144`, `:149` |
| 42 | layer -1 | -1 | `:144` |
| 43 | layer 27 | -1 | `:144` |
| 44 | variant: layer 1 is dense | 0 | `:144` (`lead = 2`) |
| 45 | variant: layer 2 is MoE | 1 | `:144` |

## Global and per-layer tensors (cases 46-51)

| # | case | expected | native anchor |
|---|---|---|---|
| 46 | `token_embd` | `{2304,163840,1}` | `:37` |
| 47 | `output_norm` | `{2304,1,1}` | `:40` |
| 48 | `output` | `{2304,163840,1}` | `:41` |
| 49 | `attn_norm` on a KDA layer | `{2304,1,1}` | `:46` |
| 50 | `attn_norm` on an MLA layer | `{2304,1,1}` | `:46` |
| 51 | `ffn_norm` | `{2304,1,1}` | `:137` |

## MLA layer, default layout (cases 52-60)

| # | case | expected | native anchor |
|---|---|---|---|
| 52 | `wq` (no q compression) | `{2304,6144,1}` | `:118-120` |
| 53 | `wkv_a_mqa` | `{2304,576,1}` | `:126` |
| 54 | `attn_kv_a_norm` | `{512,1,1}` | `:113` |
| 55 | `wk_b` | `{128,512,32}` | `:131` |
| 56 | `wv_b` | `{512,128,32}` | `:132` |
| 57 | `wo` | `{4096,2304,1}` | `:134` |
| 58 | `wq_a` absent | 0 | `:115-121` |
| 59 | `attn_q_a_norm` absent | 0 | `:112` |
| 60 | `wkv_b` absent on the split path | 0 | `:128-130` |

## KDA layer (cases 61-75)

| # | case | expected | native anchor |
|---|---|---|---|
| 61 | `ssm_q_conv` | `{4,1,4096}` | `:59-62` |
| 62 | `ssm_k_conv` | `{4,1,4096}` | `:65-68` |
| 63 | `ssm_v_conv` | `{4,1,4096}` | `:69-72` |
| 64 | `wq` | `{2304,4096,1}` | `:76` |
| 65 | `wk` | `{2304,4096,1}` | `:76` |
| 66 | `wv` | `{2304,4096,1}` | `:76`, `:54-55` |
| 67 | `ssm_f_a` | `{2304,128,1}` | `:80` |
| 68 | `ssm_f_b` | `{128,4096,1}` | `:81` |
| 69 | `ssm_beta` | `{2304,32,1}` | `:84` |
| 70 | `ssm_a` (A_log) | `{1,32,1}` | `:87-89` |
| 71 | `ssm_dt` bias | `{4096,1,1}` | `:93` |
| 72 | `ssm_g_a` | `{2304,128,1}` | `:96` |
| 73 | `ssm_g_b` | `{128,4096,1}` | `:97` |
| 74 | `ssm_o_norm` | `{128,1,1}` | `:100` |
| 75 | `wo` | `{4096,2304,1}` | `:103` |

## Dense FFN and MoE (cases 76-88)

| # | case | expected | native anchor |
|---|---|---|---|
| 76 | dense `ffn_gate` | `{2304,9216,1}` | `:146` |
| 77 | dense `ffn_down` | `{9216,2304,1}` | `:147` |
| 78 | dense `ffn_up` | `{2304,9216,1}` | `:148` |
| 79 | dense tensors absent on a MoE layer | 0 | `:144` |
| 80 | router absent on the dense layer | 0 | `:149` |
| 81 | `ffn_gate_inp` | `{2304,256,1}` | `:151` |
| 82 | `ffn_gate_exps` | `{2304,1024,256}` | `:152` |
| 83 | `ffn_down_exps` | `{1024,2304,256}` | `:153` |
| 84 | `ffn_up_exps` | `{2304,1024,256}` | `:154` |
| 85 | `ffn_gate_shexp` | `{2304,1024,1}` | `:160` |
| 86 | `ffn_down_shexp` | `{1024,2304,1}` | `:161` |
| 87 | `ffn_up_shexp` | `{2304,1024,1}` | `:162` |
| 88 | `ffn_exp_probs_b` | `{256,1,1}` | `:164` |

## Disjoint ownership (cases 89-93)

| # | case | expected | native anchor |
|---|---|---|---|
| 89 | KDA conv absent on an MLA layer | 0 | `:53-103` vs `:105-134` |
| 90 | KDA `wq` absent on an MLA layer | 0 | same |
| 91 | `wkv_a_mqa` absent on a KDA layer | 0 | same |
| 92 | `wk_b` absent on a KDA layer | 0 | same |
| 93 | `exp_probs_b` absent on the dense layer | 0 | `:164` |

## Legacy / compressed variant (cases 94-103)

| # | case | expected | native anchor |
|---|---|---|---|
| 94 | `wq_a` | `{2304,1024,1}` | `:116` |
| 95 | `attn_q_a_norm` | `{1024,1,1}` | `:112` |
| 96 | `wq_b` | `{1024,6144,1}` | `:117` |
| 97 | `wq` absent when q lora exists | 0 | `:115-121` |
| 98 | combined `wkv_b` | `{512,8192,1}` | `:128-129` |
| 99 | `wk_b` absent on the combined path | 0 | `:130-132` |
| 100 | shared expert with 2 experts | `{2304,2048,1}` | `:159-160` |
| 101 | shared expert down with 2 experts | `{2048,2304,1}` | `:159`, `:161` |
| 102 | variant dense layer 1 gate | `{2304,9216,1}` | `:144`, `:146` |
| 103 | variant dense layer 1 down | `{9216,2304,1}` | `:147` |

The gate prints `FG-ARCH11 cases=103 values=195 failures=0`; a 1-D vector is
reported as `{n,1,1}`, a 2-D matrix as `{rows,cols,1}`, and the 4-D conv weight
`{d_conv,1,d_inner,1}` by its leading three dimensions.
