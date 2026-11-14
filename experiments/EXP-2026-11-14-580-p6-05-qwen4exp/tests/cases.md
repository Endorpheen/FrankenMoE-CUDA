# qwen4exp structure cases (P6-05 architecture 2)

Oracle: the native loader declarations in `src/models/qwen4exp.cpp`. The
values are read from those lines. Parameters in `tests/fixture_arch2.cpp`:

```
n_embd 2048, n_vocab 151936, n_layer 48, n_layer_all 49,
n_head 16, n_head_kv 4, n_ff 8192, n_expert 256, used 8,
n_ff_exp declared 0 -> 1024, n_ff_shexp declared 0 -> 8192,
ssm_d_conv 4, ssm_d_inner 4096, ssm_d_state 128, ssm_dt_rank 32,
ssm_n_group 4, n_embd_head_k 128, full_attention_interval 4,
hc 4, hc_low_rank 512, indexer_n_head 4, indexer_head_size 128,
ple_head_dim 64, ple_rows 65536, ple_conv_kernel 4, PLE layer 0
```

Derived: `hc_dim = 4*2048 = 8192`, `n_ff_exp = 1024`, `key_dim = 512`,
`value_dim = 4096`, `conv_dim = 5120`, `q_width = 4096`,
attention fused QKV `= 4096 + 512 + 512 = 5120`, indexer q width `= 4*128 = 512`.

## Cases (10)

| # | case | expected | native anchor |
|---|---|---|---|
| 1 | `token_embd.weight` | `{2048, 151936, 1}` | `qwen4exp.cpp:182` |
| 2 | `hc_head_norm.weight` | `{2048, 4, 1}` | `qwen4exp.cpp:186` |
| 3 | layer 0 kind | recurrent (`1`) | `qwen4exp.cpp:173-181`; `(0+1)%4 != 0` |
| 4 | layer 3 kind | attention (`0`) | same; `(3+1)%4 == 0` |
| 5 | recurrent `attn_qkv.weight` | `{2048, 5120, 1}` | `qwen4exp.cpp:255` |
| 6 | attention fused `attn_qkv.weight` | `{2048, 5120, 1}` | `qwen4exp.cpp:243`; `llama-model.cpp:3860-3888` |
| 7 | `indexer.q_proj.weight` | `{2048, 512, 1}` | `qwen4exp.cpp:249` |
| 8 | `ffn_down_exps.weight` | `{1024, 2048, 256}` | `qwen4exp.cpp:276` |
| 9 | `hc_attn_down.weight` | `{8192, 512, 1}` | `qwen4exp.cpp:233` |
| 10 | MTP `nextn.eh_proj.weight` | `{4096, 2048, 1}` | `qwen4exp.cpp:294` |

Notes:

- There is no `output_norm` tensor in qwen4exp: the final HC mixer
  (`hc_head_norm/down/up`) is the output norm (`qwen4exp.cpp:184-188`,
  `:510-515`).
- HC tensors are per layer, two modules: attn (`:232-237`) and ffn
  (`:238-241`). The shape table exposes both families with the same widths.
- The indexer exists only on full-attention layers (`:246-252`); its
  q projection is `n_embd x (indexer_n_head * indexer_head_size)` and the k
  projection is `n_embd x indexer_head_size`.
- PLE tensors exist only on the declared PLE layer (one layer supported,
  `:265-272`); `q4_shape` returns 0 for them on other layers.
- The MTP `hnorm` and `nextn.hc_head_norm` are `[n_embd, hc]`, unlike
  qwen35moe's `[n_embd]` (`:296-300`); the draft keeps that difference.

## Expected fixture line

```
FG579_ARCH2 cases=10 values=26 failures=0
```

`26 = 8 shape cases x 3 dimensions + 2 kind cases x 1`.
