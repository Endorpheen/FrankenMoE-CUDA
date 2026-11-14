# glm5-next structure cases (P6-05 architecture 12)

Oracle: the native loader and builder declarations in
`src/models/glm5-next.cpp`. Parameters in `tests/fixture.cpp`:

```
n_embd 5120, n_vocab 154880, n_layer 45, n_layer_all 46 (one MTP block),
n_head 64, n_embd_head_k_mla 192, n_embd_head_v_mla 128, n_rot 0,
q_lora_rank 1536, kv_lora_rank 512, n_embd_head_kda 128, ssm_d_conv 4,
n_expert 256, n_expert_used 8, n_expert_shared 2, n_ff 12288,
indexer_n_head 32, indexer_head_size 128, indexer_top_k 2048,
indexer_kpool 4, indexer_kpool_select_tail 1, dsv4_hc_mult 4,
n_layer_dense_lead 3, expert_gating_func 0 (NONE -> SIGMOID),
declared n_layer_nextn 1, MTP probe found
```

`kv_heads` (one per layer, 46 entries): 0 on KDA layers, 8 on the nope-MLA
layers. Layers 3, 7, 11, ..., 43 are MLA, the rest of the trunk is KDA, and
layer 45 is MLA by declaration although the plan marks it MTP first.

`ff_exp` (one per layer, 46 entries): 0 on the three dense-lead layers, 1536 on
every routed layer and on the MTP block.

`indexer_full` (one per trunk layer, 45 entries): all 1 except layer 7, which
is a shared indexer that reuses layer 3's selection.

Derived: `d_inner = 128*64 = 8192`, `hc_dim = 4*5120 = 20480`,
`hc_mix_dim = 6*4 = 24`, `wq_b` columns `64*192 = 12288`,
`wk_b` `{192,512,64}`, `wv_b` `{512,128,64}`, `attn_out` `{8192,5120}`,
`indexer_attn_q_b` `{1536,4096}`, `kpool_ape` `{128,4}`, `shexp` width
`1536*2 = 3072`, `eh_proj` `{10240,5120}`.

## Cases (153)

Decisions (73 values):

| case | expected | native anchor |
|---|---|---|
| plan length | 46 | `:20-23`, `:82` |
| layer 0, 1, 2 kind | KDA (`0`) | `n_head_kv == 0`, `:22` |
| layer 3, 7, 43 kind | MLA (`1`) | positive KV heads, `:22` |
| layer 45 kind | MTP (`2`) | `il >= n_layer`, `:548` |
| attn kind, kv 0 / 8 | `0` / `1` | `:22` |
| layer kind at 46 / -1 | out of range (`-1`) | `:82` bounds |
| FFN layers 0, 2 | dense (`0`) | `i < n_layer_dense_lead`, `:155` |
| FFN layers 3, 44, 45 | MoE (`1`) | `:159-173`, MTP `:630` |
| FFN at 46 | out of range (`-1`) | `:82` |
| indexer full 3 / 7 / 11 | `1` / `0` / `1` | `:46-47`, `:141`, `:1069` |
| indexer full MTP 45 | `1` | `il >= n_layer()`, `:1069` |
| indexer full at 46 | out of range (`-1`) | `:20-23` |
| has indexer KDA / MLA / MTP | `0` / `1` / `1` | `:139-151` |
| kpool 4/2048, 1/2048, 4/2047 | legal / rejected / rejected | assert `:45` |
| top pool 64, 1000, kpool 8 | `64` / `512` / `64` | `:322` |
| n_sel tail on / off / capped | `259` / `256` / `2051` | `:325-329` |
| DSA mask 1000+259, 0+0 | `1259` / `0` | `:1002` |
| MLA nope 0 / 64 | legal / rejected | assert `:1046` |
| qk nope 192-0 / 192-64 | `192` / `128` | `:1042` |
| MLA cache width | `512` | `n_embd_head_v_full`, `:19` |
| wkv_a width 512+0 / 512+64 | `512` / `576` | `:134` |
| d_inner `128*64` | `8192` | `:96` |
| KDA conv state `(4-1)*8192` | `24576` | `:213-215` |
| KDA gate bounded / softplus | `1` / `0` | `:834-846` |
| HC mixer `(2+4)*4`, `(2+2)*2` | `24` / `8` | `:65` |
| HC mult 4 / 8 | `1` / `0` | assert `:53`, `:461` |
| HC mixer input width | `20480` | `:86` |
| out effective unset / declared | `5120` / `4096` | `llama-hparams.cpp:136-138` |
| out match flat / HC | `1` / `0` | assert `:564` |
| ff_exp dense / MoE / MTP | `0` / `1536` / `1536` | `llama-hparams.cpp:74-80` |
| shexp width 1536*2, 1536*1 | `3072` / `1536` | `:170-172` |
| gating NONE, SIGMOID, KEPT | `1` / `1` / `3` | `:31-33` |
| clamp shexp fallback / declared | `7` / `9` | `:36-38` |
| nextn probe present / missing / undeclared | `1` / `0` / `0` | `:548-555` |
| nextn single 1 / 2 | `1` / `0` | assert `:552` |
| eh_proj width `2*5120` | `10240` | `:176` |

Tensor shapes (80 cases, 214 values):

| case | expected | native anchor |
|---|---|---|
| `token_embd.weight` | `{5120,154880,1}` | `:71` |
| `output_norm.weight` | `{5120,1,1}` | `:73` |
| `output.weight` | `{5120,154880,1}` | `:74` |
| `attn_norm`, `ffn_norm` | `{5120,1,1}` | `:82-83` |
| `hc_attn_fn`, `hc_ffn_fn` | `{20480,24,1}` | `:86-89` |
| `hc_attn_base`, `hc_ffn_base` | `{24,1,1}` | `:87-89` |
| `hc_attn_scale`, `hc_ffn_scale` | `{3,1,1}` | `:88-90` |
| KDA `wq`, `wk`, `wv` | `{5120,8192,1}` | `create_tensor_qkv`, `:101` |
| KDA `ssm_*_conv` fallback | `{4,1,8192}` | `:99-101` |
| `ssm_f_a`, `ssm_g_a` | `{5120,128,1}` | `:105`, `:112` |
| `ssm_f_b`, `ssm_g_b` | `{128,8192,1}` | `:106`, `:113` |
| `ssm_beta` | `{5120,64,1}` | `:107` |
| `ssm_a` | `{64,1,1}` | `:109` |
| `ssm_dt_b` | `{8192,1,1}` | `:110` |
| `ssm_o_norm` | `{128,1,1}` | `:114` |
| KDA `wo` | `{8192,5120,1}` | `:119` |
| dense `ffn_gate`, `ffn_up` | `{5120,12288,1}` | `:156-158` |
| dense `ffn_down` | `{12288,5120,1}` | `:157` |
| MLA `attn_q_a_norm` | `{1536,1,1}` | `:130` |
| MLA `attn_kv_a_norm` | `{512,1,1}` | `:131` |
| MLA `wq_a` | `{5120,1536,1}` | `:132` |
| MLA `wq_b` | `{1536,12288,1}` | `:133` |
| MLA `wkv_a_mqa` | `{5120,512,1}` | `:134` |
| MLA `wk_b` | `{192,512,64}` | `:135` |
| MLA `wv_b` | `{512,128,64}` | `:136` |
| MLA `wo` | `{8192,5120,1}` | `:137` |
| `indexer_k_norm` / bias | `{128,1,1}` | `:146-147` |
| `indexer_proj` | `{5120,32,1}` | `:148` |
| `indexer_attn_k` | `{5120,128,1}` | `:149` |
| `indexer_attn_q_b` | `{1536,4096,1}` | `:150` |
| `indexer_kpool_gate` | `{5120,128,1}` | `:151` |
| `indexer_kpool_ape` | `{128,4,1}` | `:152` |
| `ffn_gate_inp` | `{5120,256,1}` | `:163` |
| `ffn_exp_probs_b` | `{256,1,1}` | `:164` |
| `ffn_gate_exps`, `ffn_up_exps` | `{5120,1536,256}` | `:166`, `:168` |
| `ffn_down_exps` | `{1536,5120,256}` | `:167` |
| `ffn_gate_shexp`, `ffn_up_shexp` | `{5120,3072,1}` | `:170`, `:172` |
| `ffn_down_shexp` | `{3072,5120,1}` | `:171` |
| MTP `nextn.eh_proj` | `{10240,5120,1}` | `:176` |
| MTP `nextn.enorm` / `hnorm` | `{5120,1,1}` | `:177-178` |
| MTP `nextn.embed_tokens` | `{5120,154880,1}` | `:179` |
| MTP `nextn.shared_head_head` | `{5120,154880,1}` | `:180` |
| MTP `nextn.shared_head_norm` | `{5120,1,1}` | `:181` |
| dense layer: router, gate_exps | absent | `:159` guard |
| KDA layer: MLA `wq_a`, `ssm_f_a` on MLA | absent | branch guards |
| shared indexer layer 7: proj, q_b, ape | absent | `:141-152`, `:1069-1073` |
| trunk layer: nextn tensor | absent | `:175` guard |
| MTP: `hc_attn_fn`, `hc_ffn_fn`, dense gate | absent | `i < n_layer`, `:85` |

Notes:

- The plan uses the KV head count exactly as the loader's `is_recr_impl`
  (`n_head_kv(i) == 0`). The fixture passes the per-layer counts directly.
- The MTP block is marked before the KV rule, because `graph_mtp` always calls
  `build_dsa_layer` and always builds MoE FFN.
- A shared (non-full) indexer layer still declares optional copies of the
  indexer tensors in native code (`TENSOR_NOT_REQUIRED`). The gate reports them
  as not owned by the plan, because no selection graph is built there.
- `n_rot` is 0 in the fixture (nope-only MLA as the graph asserts), so the
  `qk_nope` split is exercised through a decision case rather than a shape.

## Expected fixture line

```
FG-ARCH12 cases=153 values=287 failures=0
```
