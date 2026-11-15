# C1-02 report — Qwen4Exp general model graph/metadata support

Status: slice-level work complete, gate and mutations green, ready for lead
acceptance. Ledger row `C1-02` stays `TODO` until the lead integrates and
re-runs the gate on the current canon.

Scope of the report: what the canon already owned, what had no Goose
counterpart, what this slice adds, where it plugs in, and what it deliberately
does not claim.

## 1. Division of labour between canon and native

Canon `goose/model/qwen4exp.goose` (P6-05 / EXP-580) owns the plan and shape
half:

- `q4_kind(p, il, flags)` — attention / gated delta net / MTP per layer,
  `qwen4exp.cpp:93-101`.
- `q4_is_ple(p, il)` — `qwen4exp.cpp:44-62`.
- `q4_plan(p, flags, kinds)` — the 49-entry kind vector.
- `q4_shape(p, kind, tid, out)` — the declared shape of 52 tensor ids,
  `qwen4exp.cpp:109-284`.

Everything else in the file was native and had no Goose representation:

| native region | lines | was covered by canon | why not |
|---|---|---|---|
| `load_arch_hparams` | 5-107 | no | reads and validates GGUF keys; `q4_shape` only receives validated ints |
| global tensors | 109-137 | shapes only | ids had no names, no suffixes, no loader flags |
| per-layer tensors | 189-284 | shapes only | same; plus presence per layer kind is implicit in the shape switch |
| expert layout | 272-279 + `llama-model.cpp:3138` | shapes only | fused/split is a file probe, not a metadata rule |
| attention QKV | 240 + `llama-model.cpp:3146` | Q width only | fused/split is again a probe |
| model type | 103-106 | no | `n_layer() == 48` → `LLM_TYPE_A3B` |

The three new modules close rows 1, 2+5, and 3+4; the model type and the derived
recurrent pattern sit in the metadata module.

## 2. Slice M — metadata adapter (`q4m_`)

### 2.1 The 27 keys the architecture reads

Requirement classes exist because the native reader uses three forms:
`get_key(k, v)` (always required), `get_key(k, v, false)` (optional) and
`get_key` inside `if (n_ple > 0)` (required only when the PLE group is present).
Arity classes come from `get_key` vs `get_key_or_arr(k, v, 4, …)` vs
`get_key_or_arr(k, v, n_layer_all, …)` vs `get_arr`/`get_arr_n`.

| # | key suffix | required | arity |
|---:|---|---|---|
| 0 | `expert_feed_forward_length` | optional | scalar |
| 1 | `expert_shared_feed_forward_length` | optional | scalar |
| 2 | `attention.layer_norm_rms_epsilon` | always | scalar |
| 3 | `rope.dimension_sections` | always | fixed 4 |
| 4 | `ssm.conv_kernel` | always | scalar |
| 5 | `ssm.inner_size` | always | scalar |
| 6 | `ssm.state_size` | always | scalar |
| 7 | `ssm.time_step_rank` | always | scalar |
| 8 | `ssm.group_count` | always | scalar |
| 9 | `hyper_connection.count` | always | scalar |
| 10 | `hyper_connection.low_rank` | always | scalar |
| 11 | `attention.indexer.head_count` | always | scalar |
| 12 | `attention.indexer.key_length` | always | scalar |
| 13 | `attention.indexer.top_k` | always | scalar |
| 14 | `attention.compress_ratios` | optional | n_layer_all |
| 15 | `ple.layers` | optional | variable |
| 16 | `ple.ngram_size` | PLE group | scalar |
| 17 | `ple.heads_per_ngram` | PLE group | scalar |
| 18 | `ple.conv_kernel` | PLE group | scalar |
| 19 | `ple.layer_multipliers` | PLE group | variable |
| 20 | `ple.head_offsets` | PLE group | variable |
| 21 | `ple.head_vocab_sizes` | PLE group | variable |
| 22 | `ple.eos_token_id` | PLE group | scalar |
| 23 | `ple.image_token_id` | optional | scalar |
| 24 | `embedding_length_per_layer_input` | PLE group | scalar |
| 25 | `attention.recurrent_layers` | optional | n_layer_all |
| 26 | `full_attention_interval` | optional | scalar |

`q4m_key_full` prefixes them with `qwen4exp.` so a fixture compares the whole
GGUF key, not just the suffix.

### 2.2 Rules that only exist as control flow

`q4m_ple_error` reproduces the order of `qwen4exp.cpp:44-72` and returns a code
instead of throwing:

```
0 ok
1 PLE layer out of range          (throw, qwen4exp.cpp:49-51)
2 more than one PLE layer         (GGML_ASSERT(n_ple == 1))
3 conv kernel or per-layer embedding not positive (GGML_ASSERT, :62)
4 n-gram size outside [2, 8]      (throw, :66-68)
5 head count outside [1, 64]      (throw, :69-71)
```

Two details are load-bearing and are why this is a function and not four
predicates:

- `ple_n_heads = (ngram - 1) * heads_per_ngram` is computed **before** the
  n-gram range check. In the native code it is `uint32_t` arithmetic, so a
  zero n-gram wraps; `q4m_ple_heads` computes in `i64` and returns `-8` for
  `ngram = 0`, and `q4m_ple_error` still reports `4`, because the range check
  runs first.
- the conv/embedding assert fires before the n-gram check, so a file with both
  defects reports the conv one (`q4m_ple_error(1,0,49,0,0,0,0) == 3`).

The head range checks are two separate native loops in two different functions
(hparams load at `:82-90`, tensor load at `:132-137`) and are therefore two
functions here:

- `q4m_ple_head_index_errors`: `vocab == 0 || offset > INT32_MAX ||
  vocab > INT32_MAX || offset + vocab > INT32_MAX`, first offending head or -1.
- `q4m_ple_row_errors`: `offset + vocab > rows` with `rows` read back from the
  padded table, first offending head or -1.

### 2.3 Other metadata decisions

| function | native | decision |
|---|---|---|
| `q4m_hc_error` | `:27-31` | `hc > 0 && hc_low_rank > 0` |
| `q4m_indexer_error` | `:33-36` | heads, head size and top-k all `> 0` |
| `q4m_ssm_error` | `:14-18` | conv, inner, state, dt rank, group count all `> 0` |
| `q4m_embd_out` | `llama-hparams.cpp:111-113` | `n_embd_out_impl = hc * n_embd` |
| `q4m_recr_flag` | `:93-101` | `il < n_layer && (il + 1) % interval != 0`; `-1` when `interval <= 0` (the native assert); MTP layers are never recurrent |
| `q4m_interval_default` | `:94` | `4` |
| `q4m_ple_conv_state` | `llama-hparams.cpp:243-250` | `(kernel - 1) * ngram * hc * n_embd`, `0` without a PLE module |
| `q4m_type` | `:103-106` | `n_layer == 48` → A3B |

`q4m_recr_flag` is not a duplicate of `q4_kind`: `q4_kind` answers "which block
do I build" and maps MTP to its own kind, while this answers "is the native
recurrent flag set", which is `0` for MTP layers. The two agree on the trunk.

## 3. Slice T — tensor map (`q4t_`)

52 ids, the canon ids, each with a name pattern, a layer arity, a suffix and
loader flags. The names are what `llama-arch.cpp` spells, including three that
are easy to get wrong:

- `hc_head_*` are output-bucket tensors named `output_hc_norm/down/up`
  (`llama-arch.cpp:521-523`), not `hc_head_norm`.
- `per_layer_token_embd` is global (`llama-arch.cpp:548`), not `blk.%d.*`.
- `ssm_dt` is the only `bias` (`qwen4exp.cpp:256`) and `ssm_a` is created with
  **no suffix at all** (`tn(LLM_TENSOR_SSM_A_NOSCAN, il)`), so its tensor is
  `blk.N.ssm_a`, not `blk.N.ssm_a.weight`.

Flags carried: `output` is `TENSOR_NOT_REQUIRED` with a duplicate-token_embd
fallback (`q4t_output_fallback`), `per_layer_token_embd` is `TENSOR_READ_LAZY`,
`ffn_gate_up_exps` is `TENSOR_NOT_REQUIRED`, and the two fused QKV forms carry
`TENSOR_SKIP_IF_VIRTUAL`.

`q4t_present(kind, tid)` is the presence contract without the arithmetic. The
join gate checks it against `q4_shape` for every kind and id, so an id can never
mean one tensor in the shape module and another in the map.

## 4. Slice E — expert and token-mixer layout (`q4e_`)

- Widths: `q4e_n_ff_exp` / `q4e_n_ff_shexp` delegate to the accepted base
  helpers, so `n_ff_exp = declared ? declared : n_ff / n_expert_used` and
  `n_ff_shexp = declared ? declared : n_ff` cannot drift from the family
  baseline.
- Routed: `down {n_ff_exp, n_embd, n_expert}`, fused
  `gate_up {n_embd, 2*n_ff_exp, n_expert}`, split
  `gate {n_embd, n_ff_exp, n_expert}`.
- Shared: `gate_inp {n_embd}`, `gate`/`up {n_embd, n_ff_shexp}`,
  `down {n_ff_shexp, n_embd}`.
- Probes: `q4e_gate_up_mode`, `q4e_qkv_mode`, `q4e_routed_count`.
- Attention: `q4e_attn_widths` gives `head_k`, `head_v = head_k`,
  `q = 2 * head_k * n_head` (query and gate interleaved), `k = v =
  head_k * n_head_kv`; `q4e_qkv_width` sums the three for the fused row;
  `q4e_head_dim_ok` is the native `GGML_ASSERT(n_embd_head_v == n_embd_head_k)`.

`q4e_experts_on(kind)` records that the expert block is created outside the
attention/recurrent branch, so every built layer has it — including MTP.

## 5. Verification summary

| gate | cases | values | failures `-O1` | failures `-O0` |
|---|---:|---:|---:|---:|
| `q4m` | 15 | 222 | 0 | 0 |
| `q4t` | 13 | 528 | 0 | 0 |
| `q4e` | 12 | 28 | 0 | 0 |
| `join` | 3 | 156 | 0 | 0 |

| mutations | half 1 | half 2 | total |
|---|---|---|---|
| `q4m` | 6/6 | 6/6 | 12/12 |
| `q4t` | 6/6 | 6/6 | 12/12 |
| `q4e` | 6/6 | 6/6 | 12/12 |

Every catch is a gate mismatch, none an abort. Case-level detail is in
`tests/cases.md`; raw output in `results/`.

## 6. Integration step

1. Copy the three modules to `goose/model/{q4m-metadata,q4t-tensor-map,q4e-expert-layout}.goose`
   unchanged.
2. Add `q4m_fail` and `q4t_fail` to the C glue (the two `fail` externs); `q4e`
   uses `fg200g_fail` through `base/model_graph.goose`.
3. Read hparams through the key table in order, then run the guards in native
   order; then derive `n_embd_out`, the recurrent pattern and the model type.
4. Join id → name/flags by `q4t_*` at tensor creation and keep `q4_shape` for the
   extents; use `q4t_present` to decide what a layer kind declares.
5. Probe the file once per fused/split decision and pass the result to
   `q4e_gate_up_mode` / `q4e_qkv_mode`.
6. Re-run `bash tests/build.sh {q4m,q4t,q4e,join}` and
   `python3 tests/mutate.py {q4m,q4t,q4e} 0 12` on the current canon before
   accepting.

Nothing in step 3-5 changes an existing call site's shape: the map is additive
to `q4_shape`, and the metadata functions take plain integers, so the adapter
can be introduced key by key.

## 7. Boundaries and risks

1. `T_NEXTN_HC_HEAD_*` names are taken from the integration tree
   (`work/llama.cpp-integration/src/llama-arch.cpp:578-580`); the pinned
   `upstream/llama.cpp` has no such tensors and no MTP block in `qwen4exp.cpp`.
   MTP ids 46..51 therefore describe the integration tree, not the pin. They are
   gated in their own case group so the two sources never blur.
2. The integration tree's `load_ngram` PLE-skip branch (creating skipped PLE
   tensors on non-PLE layers) has no upstream counterpart and is not modelled.
   It needs its own decision function before the loader depends on it.
3. HC gamma rank: canon declares `{n_embd, hc}`, native declares `{hc_dim}`.
   Same element count and the same loaded bytes; the graph reshapes before use.
   Recorded, not reconciled — reconciling it changes canon and needs its own gate.
4. The gate is structural: the oracle is a transcription of the native lines, no
   native library is linked, no weights are involved. It proves decisions, names
   and ordering, not a real GGUF load, and makes no performance claim.
5. `q4m_recr_flag` returns `-1` for a non-positive interval instead of aborting;
   an integrator that ignores the code would silently build an attention-only
   trunk. The acceptance criterion is that the adapter treats `-1` as fatal, as
   the native assert would.
6. Risk on integration: the key table is a snapshot of the pinned
   `llama-arch.cpp`. If the pin moves, `q4m_key_count` and the fixtures have to
   move with it; that is one table and one fixture, not the modules.

## 8. Hashes

See `README.md` §8 — the same SHA-256 list, including the byte-identical
`-O1`/`-O0` gate outputs.
