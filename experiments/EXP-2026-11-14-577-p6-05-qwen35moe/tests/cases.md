# qwen35moe structure cases (P6-05 architecture 1)

Oracle: the native loader declarations in `src/models/qwen35moe.cpp`. The
values below are read from those lines, not invented. Parameters are the
Apex-like set in `tests/fixture.cpp`:

```
n_embd 2048, n_vocab 151936, n_layer 40, n_layer_all 41,
n_head 16, n_head_kv 4, n_ff 8192, n_expert 256, used 8,
n_ff_exp declared 0 -> 1024, n_ff_shexp declared 0 -> 8192,
ssm_d_conv 4, ssm_d_inner 4096, ssm_d_state 128, ssm_dt_rank 32,
ssm_n_group 4, n_embd_head_k 128, full_attention_interval 4
```

Derived geometry:

- `key_dim = 128 * 4 = 512`
- `head_v_dim = 4096 / 32 = 128`, `value_dim = 128 * 32 = 4096`
- `conv_dim = 2*512 + 4096 = 5120`
- `q_gqa = 128 * 4 = 512`, `q_width = 128 * 16 * 2 = 4096`
- attention fused QKV width `= 4096 + 512 + 512 = 5120`

## Cases (10)

| # | case | expected | native anchor |
|---|---|---|---|
| 1 | `token_embd.weight` | `{2048, 151936, 1}` | `qwen35moe.cpp:43` |
| 2 | `output_norm.weight` | `{2048, 1, 1}` | `qwen35moe.cpp:46` |
| 3 | layer 0 kind | recurrent (`1`) | recurrence init `qwen35moe.cpp:18-21`; `(0+1)%4 != 0` |
| 4 | layer 3 kind | attention (`0`) | same; `(3+1)%4 == 0` |
| 5 | recurrent `attn_qkv.weight` | `{2048, 5120, 1}` | `qwen35moe.cpp:86` (`{n_embd, key_dim*2+value_dim}`) |
| 6 | recurrent `ssm_conv1d.weight` | `{4, 5120, 1}` | `qwen35moe.cpp:88` (`{ssm_d_conv, conv_dim}`) |
| 7 | attention fused `attn_qkv.weight` | `{2048, 5120, 1}` | `qwen35moe.cpp:74`; shape rule `llama-model.cpp:3860-3888` |
| 8 | attention `attn_q_norm.weight` | `{128, 1, 1}` | `qwen35moe.cpp:78` |
| 9 | `ffn_down_exps.weight` | `{1024, 2048, 256}` | `qwen35moe.cpp:96` |
| 10 | MTP `nextn.eh_proj.weight` | `{4096, 2048, 1}` | `qwen35moe.cpp:133` |

Notes:

- The two kind cases pin the interval formula `(il+1) % 4`, which is what the
  loader uses when `LLM_KV_ATTENTION_RECURRENT_LAYERS` is absent
  (`qwen35moe.cpp:18-21`). With a native flags array the port calls the
  existing `is_recr` from `base/model_graph.goose` instead.
- `ffn_down_exps` is the derived expert width: declared 0 at line 57 means
  `n_ff / n_expert_used = 8192/8 = 1024`; the tensor is
  `{n_ff_exp, n_embd, n_expert}`.
- The attention QKV row is the fused `ATTN_QKV` tensor with
  `q_width + n_embd_k_gqa + n_embd_v_gqa` columns; if a checkpoint carries
  separate Q/K/V, the same widths split into three tensors (not in this case
  set).
- The MTP row is the only NextN case; the other NextN tensors (`enorm`,
  `hnorm`, `embed_tokens`, `shared_head_*`) are in `q35_shape` and were checked
  by inspection against lines 134-140.

## Expected fixture line

```
FG579_ARCH1 cases=10 values=26 failures=0
```

`26 = 8 shape cases x 3 dimensions + 2 kind cases x 1`

## Extended coverage (mutation gate)

The original ten cases are kept verbatim and keep their own summary line. A
second phase, `FG579_ARCH1_EXT`, raises the gate to the whole plan and every
tensor branch, so the twelve mutations in `tests/mutate.py` have somewhere to
fail. All expected values remain native declarations from
`src/models/qwen35moe.cpp:54-146`.

| group | cases | what it pins |
|---|---|---|
| plan | 1 | all 41 layers: every 4th full attention, layer 40 is MTP |
| kind edges | 3 | MTP index 40, negative index, index past the plan |
| flags path | 2 | per-layer u32 marker recurrent/attention wins over the interval |
| trunk shared | 3 | `output`, `attn_norm`, `attn_post_norm` |
| attention | 2 | `attn_out` (`{n_embd_head_k*n_head, n_embd}`), `attn_k_norm` |
| routed experts | 4 | `ffn_gate_inp`, `ffn_gate_up_exps`, `ffn_gate_exps`, `ffn_up_exps` |
| shared experts | 4 | `ffn_gate_inp_shexp`, gate/up/down shexp |
| GDN branch | 7 | `wqkv_gate`, `ssm_dt`, `ssm_a`, `ssm_beta`, `ssm_alpha`, `ssm_norm`, `ssm_out` |
| MTP branch | 9 | fused QKV/out, q/k norm, `enorm`, `hnorm`, `embed_tokens`, `shared_head_head`, `shared_head_norm` |
| ownership | 4 | a kind that does not own a tensor returns 0 |
| declared widths | 4 | `n_ff_exp=1536`, `n_ff_shexp=1024` override the derived values |

Expected extended line:

```
FG579_ARCH1_EXT cases=43 values=149 failures=0
```

`149 = 33 present shapes x 3 + 4 absent x 1 + 5 kind cases x 1 + 1 plan x 41`.
The fixture exits non-zero when either phase fails.
