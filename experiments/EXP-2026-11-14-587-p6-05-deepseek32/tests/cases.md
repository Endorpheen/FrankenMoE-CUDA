# deepseek32 structure cases (P6-05 architecture 8)

Oracle: the native loader declarations in `src/models/deepseek32.cpp`. The
values are read from those lines. Parameters in `tests/fixture.cpp`:

```
n_embd 7168, n_vocab 129280, n_layer 61, n_layer_all 62, n_layer_nextn 1,
n_head 128, n_ff 18432, n_expert 256, n_expert_used 8,
n_ff_exp declared 2048, n_expert_shared 2, n_layer_dense_lead 3,
q_lora_rank 1536, kv_lora_rank 512,
n_embd_head_k_mla 192, n_embd_head_v_mla 128, n_rot 64,
indexer_n_head 64, indexer_head_size 128, indexer_top_k 2048,
expert_gating_func 0 (softmax), has_output 1
```

`n_expert_shared` is 2 in the fixture (the released model uses 1) so the
shared-expert width multiplier `n_ff_exp * n_expert_shared` is exercised.
Derived: `nope = 192 - 64 = 128`, `wq_b` width `128 * 192 = 24576`,
`wkv_a_mqa` width `512 + 64 = 576`, `wo` rows `128 * 128 = 16384`,
indexer q width `64 * 128 = 8192`, shared-expert width `2048 * 2 = 4096`.

## Plan (7 cases)

| # | case | expected | native anchor |
|---|---|---|---|
| 1 | layer 0 kind | dense (`0`) | `deepseek32.cpp:109`; `0 < 3` |
| 2 | layer 2 kind | dense (`0`) | same; `2 < 3` |
| 3 | layer 3 kind | MoE (`1`) | `:109`; `3 >= 3` |
| 4 | layer 60 kind | MoE (`1`) | same |
| 5 | layer 61 kind | MTP + MoE (`3`) | `:81-83`, `:109`; `61 >= 61`, `61 >= 3` |
| 6 | layer 62 kind | `-1` | `:81`; past `n_layer_all` |
| 7 | `ds3_plan` | `62`, kinds `0,0,1,3` | `:81-83`, `:109`; fills all layers |

## Global tensors (3 cases)

| # | case | expected | native anchor |
|---|---|---|---|
| 8 | `token_embd.weight` | `{7168, 129280, 1}` | `:71` |
| 9 | `output_norm.weight` | `{7168, 1, 1}` | `:74` |
| 10 | `output.weight` | `{7168, 129280, 1}` | `:76` (optional, tied) |

## MLA tensors (10 cases)

| # | case | expected | native anchor |
|---|---|---|---|
| 11 | `attn_norm.weight` | `{7168, 1, 1}` | `:86` |
| 12 | `attn_q_a_norm.weight` | `{1536, 1, 1}` | `:87` |
| 13 | `attn_kv_a_norm.weight` | `{512, 1, 1}` | `:88` |
| 14 | `wq_a.weight` | `{7168, 1536, 1}` | `:90` |
| 15 | `wq_b.weight` | `{1536, 24576, 1}` | `:91` |
| 16 | `wkv_a_mqa.weight` | `{7168, 576, 1}` | `:93` |
| 17 | `wk_b.weight` | `{128, 512, 128}` | `:96` |
| 18 | `wv_b.weight` | `{512, 128, 128}` | `:97` |
| 19 | `wo.weight` | `{16384, 7168, 1}` | `:99` |
| 20 | `ffn_norm.weight` | `{7168, 1, 1}` | `:101` |

## DSA indexer (6 cases)

| # | case | expected | native anchor |
|---|---|---|---|
| 21 | `indexer_k_norm.weight` | `{128, 1, 1}` | `:104` |
| 22 | `indexer_k_norm.bias` | `{128, 1, 1}` | `:105` |
| 23 | `indexer_proj.weight` | `{7168, 64, 1}` | `:106` |
| 24 | `indexer_attn_k.weight` | `{7168, 128, 1}` | `:107` |
| 25 | `indexer_attn_q_b.weight` | `{1536, 8192, 1}` | `:108` |
| 26 | indexer on MTP layer 61 | `{7168, 128, 1}` | `:103-108`; created for every `i` |

## Dense lead FFN (4 cases)

| # | case | expected | native anchor |
|---|---|---|---|
| 27 | `ffn_gate.weight` (dense) | `{7168, 18432, 1}` | `:110` |
| 28 | `ffn_down.weight` (dense) | `{18432, 7168, 1}` | `:111` |
| 29 | `ffn_up.weight` (dense) | `{7168, 18432, 1}` | `:112` |
| 30 | `ffn_gate` on a MoE layer | absent (`0`) | `:109-133`; dense only on the lead |

## Routed and shared experts (8 cases)

| # | case | expected | native anchor |
|---|---|---|---|
| 31 | `ffn_gate_inp.weight` | `{7168, 256, 1}` | `:114` |
| 32 | `ffn_exp_probs_b.bias` | `{256, 1, 1}` | `:115` |
| 33 | `ffn_gate_exps.weight` | `{7168, 2048, 256}` | `:125` |
| 34 | `ffn_up_exps.weight` | `{7168, 2048, 256}` | `:127` |
| 35 | `ffn_down_exps.weight` | `{2048, 7168, 256}` | `:126` |
| 36 | `ffn_gate_shexp.weight` | `{7168, 4096, 1}` | `:130`; `2048 * 2` |
| 37 | `ffn_down_shexp.weight` | `{4096, 7168, 1}` | `:131` |
| 38 | `ffn_up_shexp.weight` | `{7168, 4096, 1}` | `:132` |

## NextN / MTP (6 cases)

| # | case | expected | native anchor |
|---|---|---|---|
| 39 | `nextn.eh_proj.weight` | `{14336, 7168, 1}` | `:137`; `2 * n_embd` |
| 40 | `nextn.enorm.weight` | `{7168, 1, 1}` | `:138` |
| 41 | `nextn.hnorm.weight` | `{7168, 1, 1}` | `:139` |
| 42 | `nextn.shared_head_head.weight` | `{7168, 129280, 1}` | `:143` |
| 43 | `nextn.shared_head_norm.weight` | `{7168, 1, 1}` | `:144` |
| 44 | `eh_proj` on trunk layer 3 | absent (`0`) | `:135-145`; nextn only past `n_layer` |

## Decisions (10 cases)

| # | case | expected | native anchor |
|---|---|---|---|
| 45 | `ds3_is_mla(192, 128)` | `1` | `llama-hparams.cpp:298-302` |
| 46 | `ds3_is_mla(0, 0)` | `0` | same |
| 47 | `ds3_require_mla(p)` | `1` | `deepseek32.cpp:53-56` |
| 48 | `ds3_indexer_used(K_MOE)` | `1` | `:203-317`; trunk executes the indexer |
| 49 | `ds3_indexer_used(K_MTP_MOE)` | `0` | `:480-486`; MTP does not execute it |
| 50 | `ds3_top_k(1024, 2048)` | `1024` | `:314-315` |
| 51 | `ds3_top_k(4096, 2048)` | `2048` | same |
| 52 | `ds3_gating(p)` | `0` (softmax) | `llama-graph.cpp:2227-2249` |
| 53 | `ds3_output_optional()` | `1` | `deepseek32.cpp:76-79` |
| 54 | `ds3_qk_split` | `{128, 64, 192}` | `:62-63`, `:322-332` |

## Expected fixture line

```
FG-ARCH8 cases=54 values=130 failures=0
```

`130 = 35 shape cases x 3 + 2 absent-marker cases x 1 + 6 kinds x 1 +
9 decision cases x 1 + plan 5 + qk-split 3`
