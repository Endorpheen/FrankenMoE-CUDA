# C1-02 gate cases

43 cases, 934 comparisons. Every expectation is a transcription of the native
lines named next to it; no native library is linked.

## q4m — metadata adapter (15 cases / 222 values)

| # | case | native anchor |
|---:|---|---|
| 1 | key count is 27 and every suffix matches the table | `llama-arch.cpp:191-337` |
| 2 | the 27 full keys are `qwen4exp.<suffix>` | same |
| 3 | requirement class of each of the 27 keys | `get_key` / `get_key(…,false)` / PLE-group form |
| 4 | arity class of each of the 27 keys | `get_key` / `get_key_or_arr(…,4,…)` / `get_key_or_arr(…,n_layer_all,…)` / `get_arr` |
| 5 | out-of-range key ids return `-1` | API contract |
| 6 | PLE group presence from `n_ple` | `qwen4exp.cpp:40-42` |
| 7 | `ple_n_heads = (ngram-1)*per_ngram`, including the `ngram = 0` wrap | `:64` |
| 8 | PLE validation order and all five outcomes, 12 configurations | `:44-72` |
| 9 | int32 head-range guards, each on a single head plus a shared vector | `:82-90` |
| 10 | padded-table row guards, fit and overflow | `:132-137` |
| 11 | conv history rows, including the two zero cases | `llama-hparams.cpp:243-250` |
| 12 | HC / indexer / SSM guards | `qwen4exp.cpp:14-36` |
| 13 | `n_embd_out_impl = hc * n_embd` | `llama-hparams.cpp:111-113` |
| 14 | recurrent flag for all 49 layers at interval 4, at interval 2, and the non-positive interval | `qwen4exp.cpp:93-101` |
| 15 | model type for 47 / 48 / 49 layers | `:103-106` |

## q4t — tensor map (13 cases / 528 values)

| # | case | native anchor |
|---:|---|---|
| 1 | id space is 52 | canon `T_LEN` |
| 2 | name pattern of the 49 pinned tensors | `llama-arch.cpp:425-575` |
| 3 | fully formatted `blk.7.<name>.<suffix>` for all 52 | `tn(...).str()` |
| 4 | layer arity from the tensor bucket | `llama-arch.cpp:709-780` |
| 5 | bucket (input / repeating / output) | same |
| 6 | declared suffix (weight / bias / none) | `qwen4exp.cpp:109-284` |
| 7 | loader flags: required, not-required, read-lazy | same |
| 8 | skip-if-virtual on the two fused QKV forms | `llama-model.cpp:3146` |
| 9 | the output fallback duplicates `token_embd` | `qwen4exp.cpp:115-118` |
| 10 | presence for 3 kinds × 52 ids | `qwen4exp.cpp:189-284` |
| 11 | PLE table presence from the head count | `qwen4exp.cpp:130-137` |
| 12 | out-of-range ids return `-1` | API contract |
| 13 | the integration-tree `nextn.hc_head_*` group, reported separately | `work/llama.cpp-integration/src/llama-arch.cpp:578-580` |

## q4e — expert and token-mixer layout (12 cases / 28 values)

| # | case | native anchor |
|---:|---|---|
| 1 | routed width for four `(declared, n_ff, used)` combinations | `qwen4exp.cpp:139` |
| 2 | shared width with and without the declared key | `:140` |
| 3 | routed down shape, three configurations | `:274` |
| 4 | fused `gate_up` shape and its doubled row | `llama-model.cpp:3139` |
| 5 | split `gate` shape keeps the expert dimension | `llama-model.cpp:3141` |
| 6 | `gate_up` probe decides fused (2 tensors) or split (3) | `llama-model.cpp:3138-3144` |
| 7 | the four shared-expert slots, with `n_embd ≠ n_ff_shexp` | `qwen4exp.cpp:272-279` |
| 8 | element counts for the shared and routed sets | same |
| 9 | attention widths for three head configurations | `qwen4exp.cpp:240-241` |
| 10 | fused QKV row width and the probe | `llama-model.cpp:3149` |
| 11 | head-width equality assert, both directions | `qwen4exp.cpp:281` |
| 12 | experts exist on every built layer kind | `qwen4exp.cpp:272` loop placement |

## join — id-space contract (3 cases / 156 values)

| # | case |
|---:|---|
| 1 | the map's id count equals the canon's |
| 2 | for 3 kinds × 51 ids, `q4_shape` "owned" equals `q4t_present` "declared" |
| 3 | the PLE table: canon's metadata triple and the map's head count agree on and off |

Id 5 is excluded from case 2 because the PLE table is global and metadata-gated;
case 3 covers it.

## Not covered

- `q4_plan`, `q4_kind` and `q4_is_ple` themselves — inherited gates from EXP-580,
  not re-run here.
- The `is_recr` positive direction (a non-zero recurrent flag array) — the
  metadata slice derives the pattern instead; the array form is EXP-580's gap.
- Any native numeric execution, any GGUF file, any weights.
