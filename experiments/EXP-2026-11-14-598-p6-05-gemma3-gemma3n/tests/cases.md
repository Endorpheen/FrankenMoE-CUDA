# gemma3 and gemma3n structure cases (P6-05 architectures 16 and 17)

Oracle: the native loader declarations in `src/models/gemma3.cpp` and
`src/models/gemma3n.cpp`, the altup/laurel tensors at `gemma3n.cpp:26-29` and
`:65-76`, `src/llama-hparams.cpp:8-25` (`set_swa_pattern`) and
`:295-304` (`has_kv`), and `src/llama-model.cpp:3860-3896`
(`create_tensor_qkv`).

Both fixtures are structural: they call the Goose plan/shape/decision
functions and compare the results with the loader declarations. No native
library is linked.

## Parameters

gemma3 (`tests/fixture_arch16.cpp`, 4B geometry, `gemma3.cpp:20-28`):

```
n_embd 2560, n_vocab 262144, n_layer 34,
n_head 8, n_head_kv 4, n_ff 10240,
n_embd_head_k 256, n_embd_head_v 256,
has_output 0 (duplicates token_embd), n_swa 1024, swa period 6,
dense_2_feat_out 0, dense_3_feat_in 0,
f_final_logit_softcapping 30.0f, f_attention_scale 1/sqrt(2560/8)
```

Derived: `q_width = 8*256 = 2048`, `kv_width = 4*256 = 1024`,
fused QKV `= 2048 + 1024 + 1024 = 4096`, `wo = {2048, 2560}`.

gemma3n (`tests/fixture_arch17.cpp`, E4B geometry, `gemma3n.cpp:3-21`):

```
n_embd 2048, n_vocab 262144, n_layer 35,
n_head 8, n_head_kv 2, n_ff 16384,
n_embd_head_k 256, n_embd_head_v 256,
has_output 0, swa period 5,
n_altup 4, i_altup_act 0, laurel_rank 64, n_embd_altup 256,
n_layer_kv_from_start 20, n_layer_sparsity 10
```

Derived: `q_width = 8*256 = 2048`, `kv_width = 2*256 = 512`,
fused QKV `= 2048 + 512 + 512 = 3072`, `wo = {2048, 2048}`,
`per_layer_tok_embd = {256*35, 262144} = {8960, 262144}`,
`per_layer_model_proj = {2048, 8960}`.

## gemma3 cases (50 cases, 116 values)

| # | case | expected | native anchor |
|---:|---|---|---|
| 1 | plan over 34 layers | all `0` (attention) | `gemma3.cpp:55-72` |
| 2-3 | kind layer 0 / layer 33 | `0` / `0` | same |
| 4 | `token_embd.weight` | `{2560, 262144, 1}` | `gemma3.cpp:39` |
| 5 | `output_norm.weight` | `{2560, 1, 1}` | `gemma3.cpp:42` |
| 6 | `output.weight` (absent) | `{2560, 262144, 1}` | `gemma3.cpp:43-48` duplicates `token_embd` |
| 7-8 | `dense_2_out`, `dense_3_out` absent | `ok=0` | `gemma3.cpp:51-52`, both features 0 |
| 9-10 | `dense_2_out` / `dense_3_out` present | `{2560, 64, 1}` / `{64, 2560, 1}` | same lines |
| 11-21 | `attn_norm`, `attn_qkv`, `attn_wo`, `attn_q_norm`, `attn_k_norm`, `attn_post_norm`, `ffn_norm`, `ffn_gate`, `ffn_up`, `ffn_down`, `ffn_post_norm` | `{2560}`, `{2560,4096}`, `{2048,2560}`, `{256}`, `{256}`, `{2560}`, `{2560}`, `{2560,10240}`, `{2560,10240}`, `{10240,2560}`, `{2560}` | `gemma3.cpp:58-71` |
| 22 | SWA type with a positive window | `1` (STANDARD) | `gemma3.cpp:4-10` |
| 23 | SWA type with a zero window | `0` (NONE) | `gemma3.cpp:12-14` |
| 24-25 | graph template follows the SWA type | `1` / `0` | `gemma3.cpp:76-80` |
| 26 | period default | `6` | `gemma3.cpp:7`; `get_key_or_arr` default |
| 27 | period from the document | `6` | `gemma3.cpp:8` |
| 28-32 | SWA mask layers 0, 4, 5, 6, 11 | `1, 1, 0, 1, 0` | `llama-hparams.cpp:8-25` |
| 33 | period 0 | every layer `1` | same, `n_pattern == 0` branch |
| 34-35 | rope base source on SWA / full layer | `1` / `0` | `gemma3.cpp:115-121` |
| 36-39 | model type 34 / 62 / 18 / 40 layers | `4B`, `27B`, `270M`, unknown | `gemma3.cpp:20-28` |
| 40-41 | attention scale kind 4B / 27B | `0` (per head) / `1` (per `n_embd/n_head`) | `gemma3.cpp:30-34` |
| 42 | qk-norm marker | `1` | `gemma3.cpp:133-141` |
| 43 | FFN activation marker | `1` (GELU-PAR) | `gemma3.cpp:184` |
| 44-45 | softcap with / without the key | `1` / `0` | `gemma3.cpp:212-216` |
| 46-47 | input scale for tokens / raw embeddings | `1` / `0` | `gemma3.cpp:93` |
| 48 | shared auto scale branch | `0` (explicit scale present) | `load_arch_hparams:31-33`, `base/model_graph.goose` |
| 49-50 | output source: duplicate / declared | `1` / `0` | `gemma3.cpp:43-48` |

## gemma3n cases (62 cases, 157 values)

| # | case | expected | native anchor |
|---:|---|---|---|
| 1 | plan over 35 layers | all `0` (attention) | `gemma3n.cpp:47-77` |
| 2-3 | kind layer 0 / layer 34 | `0` / `0` | same |
| 4 | `token_embd.weight` | `{2048, 262144, 1}` | `gemma3n.cpp:36` |
| 5 | `output_norm.weight` | `{2048, 1, 1}` | `gemma3n.cpp:45` |
| 6 | `output.weight` (absent) | `{2048, 262144, 1}` | `gemma3n.cpp:30-34` |
| 7 | `altup_proj.weight` | `{2048, 2048, 3}` | `gemma3n.cpp:38` |
| 8 | `altup_unembd_proj.weight` | `{2048, 2048, 3}` | `gemma3n.cpp:39` |
| 9 | `per_layer_tok_embd.weight` | `{8960, 262144, 1}` | `gemma3n.cpp:41` |
| 10 | `per_layer_model_proj.weight` | `{2048, 8960, 1}` | `gemma3n.cpp:42` |
| 11 | `per_layer_proj_norm.weight` | `{256, 1, 1}` | `gemma3n.cpp:43` |
| 12-22 | `attn_norm`, `attn_qkv`, `attn_wo`, `attn_q_norm`, `attn_k_norm`, `attn_post_norm`, `ffn_norm`, `ffn_gate`, `ffn_up`, `ffn_down`, `ffn_post_norm` | `{2048}`, `{2048,3072}`, `{2048,2048}`, `{256}`, `{256}`, `{2048}`, `{2048}`, `{2048,16384}`, `{2048,16384}`, `{16384,2048}`, `{2048}` | `gemma3n.cpp:50-63` |
| 23-33 | `per_layer_inp_gate`, `per_layer_proj`, `per_layer_post_norm`, `altup_correct_coef`, `altup_correct_scale`, `altup_predict_coef`, `altup_router`, `altup_router_norm`, `laurel_l`, `laurel_r`, `laurel_post_norm` | `{2048,256}`, `{256,2048}`, `{2048}`, `{4,4}`, `{2048}`, `{4,16}`, `{2048,4}`, `{2048}`, `{2048,64}`, `{64,2048}`, `{2048}` | `gemma3n.cpp:66-76` |
| 34 | KV prefix value | `20` | `gemma3n.cpp:9` |
| 35-39 | `has_kv`: layer 19 / 20 / 34 of a 20 prefix, all-layer `-1`, zero prefix | `1, 0, 0, 1, 0` | `llama-hparams.cpp:295-304` |
| 40 | SWA type | `1` (unconditional) | `gemma3n.cpp:6` |
| 41-42 | period default / document | `5` / `5` | `gemma3n.cpp:4-5` |
| 43-46 | SWA mask layers 0, 4, 5, 9 | `1, 0, 1, 0` | `llama-hparams.cpp:8-25` |
| 47-48 | attention scale bits / auto branch | `1065353216` (1.0f) / `0` | `gemma3n.cpp:10` |
| 49-50 | qk-norm marker / weightless V norm | `1` / `1` | `gemma3n.cpp:157-159` |
| 51-53 | sparsity layer count / layer 9 / layer 10 | `10`, `1`, `0` | `models.h:836`, `gemma3n.cpp:212` |
| 54 | softcap is unconditional | `1` | `gemma3n.cpp:301-306` |
| 55-58 | altup extra states / merge divisor / active index / correct bias | `3`, `4`, `0`, `1` | `gemma3n.cpp:38-39,142,283,448` |
| 59-60 | input scale for tokens / raw embeddings | `1` / `0` | `gemma3n.cpp:104` |
| 61-62 | output source: duplicate / declared | `1` / `0` | `gemma3n.cpp:30-34` |

## Expected gate lines

```
FG-ARCH16 cases=50 values=116 failures=0
FG-ARCH17 cases=62 values=157 failures=0
```

Value arithmetic: gemma3 `35 (plan) + 2 (kinds) + 48 (16 shapes x 3) + 2
(absent) + 6 (SWA mask) + 23 (markers) = 116`; gemma3n `36 (plan) + 2 (kinds)
+ 90 (30 shapes x 3) + 6 (KV) + 7 (SWA) + 16 (markers) = 157`.
