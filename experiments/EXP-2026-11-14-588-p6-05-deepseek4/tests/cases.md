# deepseek4 structure cases (P6-05 architecture 9)

Oracle: the native loader and builder declarations in
`src/models/deepseek4.cpp`. Parameters in `tests/fixture.cpp`:

```
n_embd 4096, n_vocab 129280, n_layer 43, n_layer_all 44 (one MTP block),
n_head 64, n_embd_head_k 128, n_rot 64, q_lora_rank 1536,
o_group_count 8, o_lora_rank 512, n_expert 256, n_expert_used 8,
n_ff_exp 2048, n_expert_shared 1, indexer_n_head 64, indexer_head_size 128,
indexer_top_k 2048, hc_mult 4, hash_layer_count 2, n_swa 2048,
expert_gating_func 3 (sqrtsoftplus), MTP probe found, n_layer_nextn 1
```

`compress_ratios` (one per layer, 44 entries): layers 0 and 1 are 0, every
layer with `il % 8 == 7` is 4, the rest are 128, and the MTP layer 43 is 0
because the MTP pass has no deepseek4 input (`deepseek4.cpp:940`).

Derived: `hc_dim = 4*4096 = 16384`, `hc_mix_dim = 6*4 = 24`,
`n_ff_exp = 2048`, `n_ff_shexp = 2048*1 = 2048`,
`o_group_dim = (64/8)*128 = 1024`, `wo_b` rows `8*512 = 4096`,
`wq_b` columns `64*128 = 8192`, CSA compressor width `2*128 = 256`,
HCA compressor width `128`, indexer q width `64*128 = 8192`,
indexer compressor width `2*128 = 256`.

## Cases (100)

Decisions (37 values):

| case | expected | native anchor |
|---|---|---|
| plan length | 44 | `:58-69` |
| layer 0, 1 kind | raw (`0`) | ratio 0, `:938-960` |
| layer 7, 15 kind | CSA (`1`) | ratio 4, `:1218-1225` |
| layer 2, 8 kind | HCA (`2`) | ratio 128, `:1226-1230` |
| layer 43 kind | MTP (`3`) | `il >= n_layer`, `:205` |
| ratio 0 / 4 / 128 | `0` / `1` / `2` | `:161-181` |
| ratio 16 | rejected (`-1`) | "only ratios 0, 4, 128", `:179-180` |
| FFN kind, trunk and MTP | MoE (`1`) | `:1385-1402`, `:1520-1538` |
| FFN kind, `il == n_layer_all` | out of range (`-1`) | `:205` |
| route kind, layers 0, 1 | hash (`0`) | `il < hash_layer_count`, `:1371-1374` |
| route kind, layer 2 | score (`1`) | `:1375-1376` |
| MTP route, layer 43 / 1 | legal / rejected | assert `il >= hash_layer_count`, `:1509` |
| SWA trunk, MTP (pattern 0) | `1` / `1` | `:98-105`, `llama-hparams.cpp:8-22` |
| SWA pattern 4, layers 1 / 4 | `1` / `0` | generic pattern rule |
| nextn count, probe present / missing | `1` / `0` | `:15-21` |
| HC out width `4096*4` | `16384` | `:73` |
| HC out effective / fallback | `16384` / `4096` | `llama-hparams.cpp:136-138` |
| HC out match / mismatch | `1` / `0` | assert `:1451` |
| HC mixer width `(2+4)*4` | `24` | `:125` |
| HC mult 4 / 8 | `1` / `0` | assert `hc == 4`, `:372` |
| HC head weight width | `16384` | `:131` |
| o-group divisibility 64/8, 64/5 | `1` / `0` | assert, `:888` |
| o-group dim | `1024` | `:886-887` |
| head split 128/64, 64/128 | `1` / `0` | assert, `:890` |
| sink width | `64` | `:139` |
| indexer top-k clamp 3000/2048, 1000/2048 | `2048` / `1000` | `:653` |
| gating sqrtsoftplus / softmax | `1` / `0` | `:86-89` |
| clamp shexp fallback / declared | `7` / `9` | `:38-40` |
| MTP ratio 0 / 4 | `1` / `0` | assert, `:940` |

Tensor shapes (100 cases including the decision cases above; 188 values):

| case | expected | native anchor |
|---|---|---|
| `token_embd.weight` | `{4096, 129280, 1}` | `:127` |
| `output_norm.weight` | `{4096, 1, 1}` | `:129` |
| `hc_head_fn.weight` | `{16384, 4, 1}` | `:131` |
| `hc_head_base.weight` | `{4, 1, 1}` | `:132` |
| `wq_a.weight` | `{4096, 1536, 1}` | `:140` |
| `attn_q_a_norm.weight` | `{1536, 1, 1}` | `:141` |
| `wq_b.weight` | `{1536, 8192, 1}` | `:142` |
| `wkv.weight` | `{4096, 128, 1}` | `:143` |
| `attn_kv_norm.weight` | `{128, 1, 1}` | `:144` |
| `wo_a.weight` | `{1024, 512, 8}` | `:145-147` |
| `wo_b.weight` | `{4096, 4096, 1}` | `:148` |
| `attn_sinks.weight` | `{64, 1, 1}` | `:139` |
| `hc_attn_fn.weight` | `{16384, 24, 1}` | `:154` |
| `hc_attn_base.weight` | `{24, 1, 1}` | `:155` |
| `hc_attn_scale.weight` | `{3, 1, 1}` | `:156` |
| `hc_ffn_fn/base/scale` | `{16384,24,1}` / `{24,1,1}` / `{3,1,1}` | `:157-159` |
| CSA `comp_wkv/wgate` | `{4096, 256, 1}` | `:165-166`, `coff = 2` |
| CSA `comp_ape` | `{256, 4, 1}` | `:167` |
| CSA `comp_norm` | `{128, 1, 1}` | `:168` |
| HCA `comp_wkv` | `{4096, 128, 1}` | `coff = 1` |
| HCA `comp_ape` | `{128, 128, 1}` | `:167` |
| raw / MTP compressor | absent | `:161` guard |
| `indexer_proj.weight` | `{4096, 64, 1}` | `:173` |
| `indexer_attn_q_b.weight` | `{1536, 8192, 1}` | `:174` |
| `indexer_comp_wkv/wgate` | `{4096, 256, 1}` | `:176-177` |
| `indexer_comp_ape` | `{256, 4, 1}` | `:178` |
| `indexer_comp_norm` | `{128, 1, 1}` | `:179` |
| HCA / MTP indexer | absent | `ratio == 4` guard |
| `ffn_gate_tid2eid` (hash layer 0) | `{8, 129280, 1}` | `:186-187` |
| `ffn_exp_probs_b` (score layer 2) | `{256, 1, 1}` | `:189` |
| `ffn_gate_inp.weight` | `{4096, 256, 1}` | `:184` |
| `ffn_gate_exps.weight` | `{4096, 2048, 256}` | `:193` |
| `ffn_down_exps.weight` | `{2048, 4096, 256}` | `:194` |
| `ffn_up_exps.weight` | `{4096, 2048, 256}` | `:195` |
| `ffn_gate_shexp.weight` | `{4096, 2048, 1}` | `:197` |
| `ffn_down_shexp.weight` | `{2048, 4096, 1}` | `:198` |
| `ffn_up_shexp.weight` | `{4096, 2048, 1}` | `:199` |
| MTP `nextn.eh_proj.weight` | `{8192, 4096, 1}` | `:206` |
| MTP `nextn.enorm/hnorm` | `{4096, 1, 1}` | `:207-208` |
| MTP `nextn.embed_tokens.weight` | `{4096, 129280, 1}` | `:209` |
| MTP `nextn.shared_head_head.weight` | `{4096, 129280, 1}` | `:210` |
| MTP `nextn.shared_head_norm.weight` | `{4096, 1, 1}` | `:211` |

Notes:

- Every layer is MoE: the native builder has no dense FFN branch, so
  `ds4_ffn_kind` cannot return "dense" for a valid layer.
- The plan carries the attention variant, not a hybrid interval: the ratio
  comes from the model file and the loader rejects anything but 0, 4, 128.
- The MTP block reuses the trunk attention builder with an MTP input
  (`:1206-1212`), so it has no compressor and no indexer; the plan marks it
  `K_MTP` so the shape table cannot hand it either family.
- `ffn_exp_probs_b_vl` (the vision-only routing bias) is declared for every
  layer in the loader (`:191`) and is deliberately left out of the gate: it is
  optional and the graph only reads it for media batches.
- Sinks are per query head and exist on all four kinds, which is why one
  shape case covers raw, CSA, HCA and MTP.

## Expected fixture line

```
FG-ARCH9 cases=100 values=188 failures=0
```
