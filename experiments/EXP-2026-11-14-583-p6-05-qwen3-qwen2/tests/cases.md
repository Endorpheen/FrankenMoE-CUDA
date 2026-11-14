# qwen3 and qwen2 structure cases (P6-05 architectures 3 and 6)

Oracle: the native loader declarations in `src/models/qwen3.cpp` and
`src/models/qwen2.cpp`, cross-checked with `src/llama-model.cpp:3860-3896`
(`create_tensor_qkv`) and `src/llama-model.h:985-1000` (`LLAMA_LOAD_LOCALS`).

Both fixtures are structural: they call the Goose plan/shape functions and
compare the returned dims with the loader declarations. No native library is
linked.

## Parameters

qwen3 (`tests/fixture_arch3.cpp`, 0.6B geometry, `qwen3.cpp:5-16`):

```
n_embd 1024, n_vocab 151936, n_layer 28,
n_head 16, n_head_kv 8, n_ff 3072,
n_embd_head_k 128, n_embd_head_v 128,
has_output 0 (duplicates token_embd), has_attn_bias 0,
n_cls_out 0, n_embd_out 0
```

Derived: `q_width = 16*128 = 2048`, `kv_width = 8*128 = 1024`,
fused QKV `= 2048 + 1024 + 1024 = 4096`, `wo = {2048, 1024}`.

qwen2 (`tests/fixture_arch6.cpp`, 7B geometry, `qwen2.cpp:5-20`):

```
n_embd 3584, n_vocab 152064, n_layer 28,
n_head 28, n_head_kv 4, n_ff 18944,
n_embd_head_k 128, n_embd_head_v 128,
has_output 1, has_attn_bias 1, has_output_bias 1
```

Derived: `q_width = n_embd = 3584`, `kv_width = 4*128 = 512`,
fused QKV `= 3584 + 512 + 512 = 4608`, `wo = {3584, 3584}`.

## qwen3 cases (30 cases, 96 values)

| # | case | expected | native anchor |
|---:|---|---|---|
| 1 | plan over 28 layers | all `0` (attention) | `qwen3.cpp:37-50`; plan is uniform |
| 2-3 | kind layer 0 / layer 27 | `0` / `0` | same |
| 4 | `token_embd.weight` | `{1024, 151936, 1}` | `qwen3.cpp:18` |
| 5 | `output_norm.weight` | `{1024, 1, 1}` | `qwen3.cpp:21` |
| 6 | `output.weight` (absent) | `{1024, 151936, 1}` | `qwen3.cpp:22-25` duplicates `token_embd` |
| 7-9 | `cls_out`, `dense_2_out`, `dense_2_out.bias` absent | `ok=0` | `qwen3.cpp:29-33` (`n_cls_out=0`, `n_embd_out=0`) |
| 10 | `cls_out.weight` present | `{1024, 64, 1}` | `qwen3.cpp:29` |
| 11-12 | `dense_2_out.weight` / `.bias` present | `{1024, 256, 1}` / `{256, 1, 1}` | `qwen3.cpp:32-33` |
| 13-14 | `attn_norm.weight`, `ffn_norm.weight` | `{1024, 1, 1}` | `qwen3.cpp:38,46` |
| 15 | `attn_qkv.weight` | `{1024, 4096, 1}` | `qwen3.cpp:40`; `llama-model.cpp:3863` |
| 16-17 | `attn_q_norm.weight`, `attn_k_norm.weight` | `{128, 1, 1}` | `qwen3.cpp:43-44` |
| 18 | `attn_wo.weight` | `{2048, 1024, 1}` | `qwen3.cpp:41` |
| 19-21 | `ffn_gate`, `ffn_up`, `ffn_down` | `{1024, 3072, 1}`, `{1024, 3072, 1}`, `{3072, 1024, 1}` | `qwen3.cpp:47-49` |
| 22 | qk-norm marker | `1` | `qwen3.cpp:102,111` |
| 23-24 | output source marker | duplicate `1`, declared `0` | `qwen3.cpp:23-26` |
| 25 | fused `attn_qkv.bias` absent | `ok=0` | `llama-model.cpp:3880-3887` with flag 0 |
| 26-28 | legacy `wq_b`, `wk_b`, `wv_b` | `{2048}`, `{1024}`, `{1024}` | `llama-model.cpp:3888-3895` |
| 29 | fused `attn_qkv.bias` present | `{4096, 1, 1}` | `llama-model.cpp:3881` |
| 30 | legacy `wq_b` suppressed | `ok=0` | `llama-model.cpp:3883-3887` |

## qwen2 cases (26 cases, 84 values)

| # | case | expected | native anchor |
|---:|---|---|---|
| 1 | plan over 28 layers | all `0` (attention) | `qwen2.cpp:34-50`; plan is uniform |
| 2-3 | kind layer 0 / layer 27 | `0` / `0` | same |
| 4 | `token_embd.weight` | `{3584, 152064, 1}` | `qwen2.cpp:22` |
| 5 | `output_norm.weight` | `{3584, 1, 1}` | `qwen2.cpp:25` |
| 6 | `output.weight` | `{3584, 152064, 1}` | `qwen2.cpp:26` |
| 7 | `output.bias` | `{152064, 1, 1}` | `qwen2.cpp:27` |
| 8-9 | `attn_norm.weight`, `ffn_norm.weight` | `{3584, 1, 1}` | `qwen2.cpp:36,41` |
| 10 | `attn_qkv.weight` | `{3584, 4608, 1}` | `qwen2.cpp:38`; `llama-model.cpp:3863` |
| 11 | `attn_qkv.bias` | `{4608, 1, 1}` | `qwen2.cpp:38`; `llama-model.cpp:3881` |
| 12 | `attn_wo.weight` | `{3584, 3584, 1}` | `qwen2.cpp:39` |
| 13-15 | `ffn_gate`, `ffn_up`, `ffn_down` | `{3584, 18944, 1}`, `{3584, 18944, 1}`, `{18944, 3584, 1}` | `qwen2.cpp:43-45` |
| 16-18 | `attn_q_norm` / `attn_k_norm` absent, marker `0` | `ok=0` | `qwen2.cpp:81-98` (plain RoPE) |
| 19 | `attn_wo.bias` absent | `ok=0` | `qwen2.cpp:38-50` declares none |
| 20-22 | legacy `wq_b`, `wk_b`, `wv_b` | `{3584}`, `{512}`, `{512}` | `llama-model.cpp:3888-3895` |
| 23 | fused bias suppressed when legacy is used | `ok=0` | `llama-model.cpp:3883-3887` |
| 24 | output bias optional | `ok=0` | `qwen2.cpp:27` |
| 25-26 | output source marker | declared `0`, duplicate `1` | `qwen2.cpp:28-31` |

## Expected gate lines

```
FG-ARCH3 cases=30 values=96 failures=0
FG-ARCH6 cases=26 values=84 failures=0
```

Value arithmetic: qwen3 `29 (plan) + 2 (kinds) + 57 (19 shapes x 3) + 5 (absent)
+ 3 (markers)`; qwen2 `29 (plan) + 2 (kinds) + 45 (15 shapes x 3) + 5 (absent)
+ 3 (markers)`.
