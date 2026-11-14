# glm4 / glm-dsa structure cases (P6-05 architectures 14 and 15)

Two independent gates, one per family. Oracle: the native loader and builder
declarations in `src/models/glm4.cpp` and `src/models/glm-dsa.cpp` of the
pinned llama.cpp tree. No native library is linked.

## Gate 14 — glm4

Parameters in `tests/fixture_arch14.cpp`:

```
n_embd 4096, n_vocab 151552, n_layer 40, n_layer_nextn 1, n_head 32,
n_head_kv 2, n_ff 13696, n_embd_head_k 128, n_embd_head_v 128,
output absent, no fused attention bias
```

Derived: `n_layer_all = 41`, `q_width = 32*128 = 4096`,
`k_width = v_width = 2*128 = 256`, fused `qkv = 4608`,
`ffn_up = 2*13696 = 27392`, `nextn.eh_proj` rows `2*4096 = 8192`.

| case | expected | native anchor |
|---|---|---|
| plan length | `41` | `:8-9`, `:32` |
| block 0, block 39 kind | trunk (`0`) | `il < n_layer` |
| block 40 kind | NextN (`1`) | `:54` |
| block 41 / block -1 kind | rejected (`-1`) | outside `n_layer_all` |
| `n_layer_all` | `41` | `:32` |
| NextN assert, 1 declared / 0 blocks and 0 declared | ok / rejected | `:9` |
| query width `32*128` | `4096` | `:42` |
| key / value width `2*128` | `256` / `256` | `llama-model.h` gqa widths |
| fused QKV row | `4608` | `llama-model.cpp:3149` |
| `ffn_up` fused width `2*n_ff` | `27392` | `:50` |
| NextN `eh_proj` rows | `8192` | `:56` |
| head split 128/128, 128/64 | `1` / `0` | assert `:75` |
| output from token_embd / declared | `1` / `0` | `:27-30` |
| `token_embd.weight` | `{4096, 151552, 1}` | `:22` |
| `output_norm.weight` | `{4096, 1, 1}` | `:25` |
| `output.weight` (duplicated) | `{4096, 151552, 1}` | `:26-30` |
| `attn_norm.weight` | `{4096, 1, 1}` | `:41` |
| `attn_qkv.weight` | `{4096, 4608, 1}` | `:42` |
| `attn_out.weight` | `{4096, 4096, 1}` | `:44` |
| `attn_post_norm.weight` | `{4096, 1, 1}` | `:46` |
| `ffn_norm.weight` | `{4096, 1, 1}` | `:48` |
| `ffn_up.weight` (gate + up) | `{4096, 27392, 1}` | `:50` |
| `ffn_down.weight` | `{13696, 4096, 1}` | `:49` |
| `ffn_post_norm.weight` | `{4096, 1, 1}` | `:52` |
| unknown tensor id | absent | no `ffn_gate` row exists |
| fused qkv bias | absent (flag 0) | `create_tensor_qkv` |
| legacy q / k / v bias | `{4096,1,1}` / `{256,1,1}` / `{256,1,1}` | `llama-model.cpp:3172-3174` |
| fused qkv bias (flag 1) | `{4608, 1, 1}`, legacy suppressed | `llama-model.cpp:3167` |
| NextN block: `attn_norm`, `ffn_up` | trunk rows kept | `:32-52` (TENSOR_SKIP) |
| NextN block: `eh_proj`, `enorm`, `hnorm` | `{8192,4096,1}`, `{4096,1,1}`, `{4096,1,1}` | `:56-58` |
| NextN block: `embed_tokens`, `shared_head_head`, `shared_head_norm` | `{4096,151552,1}`, `{4096,151552,1}`, `{4096,1,1}` | `:61-63` |
| trunk block: `eh_proj`, `enorm`, `shared_head_head` | absent | `:55` guard |

Expected fixture line:

```
FG-ARCH14 cases=50 values=96 failures=0
```

## Gate 15 — glm-dsa

Parameters in `tests/fixture_arch15.cpp`:

```
n_embd 5120, n_vocab 154880, n_layer 78, n_layer_nextn 1, n_head 64,
n_rot 64, q_lora_rank 1536, kv_lora_rank 512, n_embd_head_k_mla 192,
n_embd_head_v_mla 128, n_ff 13312, n_ff_exp 1536, n_expert 256,
n_expert_used 8, n_expert_shared 2, n_dense_lead 3, indexer_n_head 32,
indexer_head_size 128, indexer_top_k 2048, gating sigmoid,
output absent, n_ctx_train 1048576
```

`is_indexer_full_impl` (78 entries) is the `GLM_5_2_DEFAULT_INDEXER_TYPES`
table: full at 0 and 1, then every fourth layer from 2 on.

Derived: `nope = 192-64 = 128`, `wq_b` columns `64*192 = 12288`,
`wkv_a_mqa` `512+64 = 576`, `wo` rows `64*128 = 8192`,
shared-expert width `1536*2 = 3072`, indexer q columns `32*128 = 4096`,
`nextn.eh_proj` rows `2*5120 = 10240`.

| case | expected | native anchor |
|---|---|---|
| plan length | `79` | `:125` |
| block 0, block 77 kind | trunk (`0`) | `il < n_layer` |
| block 78 kind | NextN (`1`) | `:181` |
| block 79 / block -1 kind | rejected (`-1`) | outside `n_layer_all` |
| `n_layer_all` | `79` | `:125` |
| NextN assert / single draft block / two declared | ok / ok / rejected | `:61`, `:548` |
| FFN kind, layers 0, 2 / 3, 77, 78 / 79 | dense / MoE / rejected | `:155`, `:125` |
| MTP FFN agreement, dense lead 3 / 79 | `1` / `0` | `:155` vs assert `:560` |
| MLA present / half-declared | `1` / `0` | `:98-101`, `llama-hparams.cpp:272-277` |
| MLA pair both set / half / both unset | `1` / `0` / `1` | assert `llama-hparams.cpp:273-274` |
| nope width | `128` | `:108` |
| `wo` rows / `wq_b` columns | `8192` / `12288` | `:145`, `:137` |
| indexer back-compat, 128K / 1M context | `1` / `0` (all full / table) | `:63-69` |
| 5.2 pattern layers 0, 2, 6 / layer 3 | full / shared | `:6-27` |
| indexer kind from the table, layers 0 / 3 / 78 | `1` / `0` / `-1` | `llama-hparams.cpp:279-285` |
| indexer ok, full / shared after full / first shared | `1` / `1` / `0` | assert `:370` |
| top-k clamp 3000/2048, 1000/2048 | `2048` / `1000` | `:364` |
| rope inside indexer head 64/128, 192/128 | `1` / `0` | assert `:220` |
| gating absent / declared | `2` (sigmoid) / kept | `:53-57` |
| shared-expert width `n_ff_exp*n_expert_shared` | `3072` | `:176-178` |
| MoE dims with `n_expert_used` 8 / 0 | `1` / `0` | `:163-168` |
| output from token_embd / declared | `1` / `0` | `:121-123` |
| `nextn.eh_proj` rows | `10240` | `:183` |
| `token_embd.weight` | `{5120, 154880, 1}` | `:115` |
| `output_norm.weight` | `{5120, 1, 1}` | `:118` |
| `output.weight` (duplicated) | `{5120, 154880, 1}` | `:120-123` |
| `attn_norm.weight` | `{5120, 1, 1}` | `:132` |
| `attn_q_a_norm.weight` | `{1536, 1, 1}` | `:133` |
| `attn_kv_a_norm.weight` | `{512, 1, 1}` | `:134` |
| `wq_a.weight` | `{5120, 1536, 1}` | `:136` |
| `wq_b.weight` | `{1536, 12288, 1}` | `:137` |
| `wkv_a_mqa.weight` | `{5120, 576, 1}` | `:139` |
| `wk_b.weight` | `{128, 512, 64}` | `:142` |
| `wv_b.weight` | `{512, 128, 64}` | `:143` |
| `attn_out.weight` | `{8192, 5120, 1}` | `:145` |
| `indexer_k_norm.weight` / `.bias` | `{128, 1, 1}` / `{128, 1, 1}` | `:150-151` |
| `indexer_proj.weight` | `{5120, 32, 1}` | `:152` |
| `indexer_attn_k.weight` | `{5120, 128, 1}` | `:153` |
| `indexer_attn_q_b.weight` | `{1536, 4096, 1}` | `:154` |
| indexer rows on a dense lead layer | present | `:149-154` (outside the FFN branch) |
| dense `ffn_norm` / `ffn_gate` / `ffn_up` / `ffn_down` | `{5120,1,1}` / `{5120,13312,1}` / `{5120,13312,1}` / `{13312,5120,1}` | `:147`, `:156-158` |
| dense layer: router absent | absent | `:159-179` guard |
| `ffn_gate_inp.weight` | `{5120, 256, 1}` | `:160` |
| `ffn_exp_probs_b.weight` | `{256, 1, 1}` | `:161` |
| `ffn_gate_exps` / `ffn_up_exps` | `{5120, 1536, 256}` | `:171`, `:173` |
| `ffn_down_exps` | `{1536, 5120, 256}` | `:172` |
| `ffn_gate_shexp` / `ffn_up_shexp` | `{5120, 3072, 1}` | `:176`, `:178` |
| `ffn_down_shexp` | `{3072, 5120, 1}` | `:177` |
| MoE layer: dense `ffn_gate` absent | absent | `:155` branch |
| NextN block: trunk rows kept, router present | present | `:125-179` |
| NextN: `eh_proj` / `enorm` / `hnorm` | `{10240,5120,1}` / `{5120,1,1}` / `{5120,1,1}` | `:183-185` |
| NextN: `embed_tokens` / `shared_head_head` / `shared_head_norm` | `{5120,154880,1}` / `{5120,154880,1}` / `{5120,1,1}` | `:188-190` |
| trunk block: `eh_proj`, `shared_head_head` | absent | `:182` guard |

Expected fixture line:

```
FG-ARCH15 cases=94 values=170 failures=0
```

## Notes

- The plan carries only the trunk/NextN split, because that is the only block
  kind either family has. glm-dsa's FFN branch is an index decision
  (`il < n_layer_dense_lead`), so it is a separate function that the shape
  table consults; it is not a plan kind.
- glm4's NextN block keeps its trunk attention and FFN rows in the file with
  `TENSOR_SKIP`, and the forward pass never runs it. The gate therefore checks
  that those rows are present with trunk shapes and that only the nextn rows
  are kind-gated.
- glm-dsa's MTP block loads whatever the index rule says, but the MTP graph
  always builds a MoE FFN and asserts `ffn_gate_inp`. `dsa_mtp_ffn_ok` records
  the only combination that agrees; the fixture exercises both answers.
- Both fixtures use one parameter set per family. Other sizes differ only by
  the same arithmetic and are not gated.
