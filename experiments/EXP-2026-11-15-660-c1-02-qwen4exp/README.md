# C1-02 — extending the general model graph/metadata path to Qwen4Exp

Base commit: `e4115311` (the repository advanced to `2b443d45` while this slice
was being built; the Goose inputs used here are byte-identical at both commits,
see "Base and inputs").

Ledger row: `docs/goose-migration/tasks.json` → `C1-02`
("Extend general model graph/metadata support to Qwen4Exp"), still `TODO` — this
slice supplies evidence for one part of its `remaining` list
("general model graph/metadata builders needed by Qwen").

No production file was touched, no model or GPU was started, and no build
outside this directory was run.

## 1. Inventory: what the canon already covers, and what it does not

The accepted `goose/model/qwen4exp.goose` (P6-05, EXP-580) is the plan and shape
half of the builder. It exports exactly four functions:

| export | covers | native anchor |
|---|---|---|
| `q4_kind` | per-layer kind: attention / gated delta net / MTP | `qwen4exp.cpp:93-101` |
| `q4_is_ple` | whether a layer carries PLE | `qwen4exp.cpp:44-62` |
| `q4_plan` | the whole 49-layer kind vector | same as `q4_kind` |
| `q4_shape` | declared shape for 52 tensor ids, per kind | `qwen4exp.cpp:109-284` |

So the *arithmetic* of the file was already in Goose. What had no Goose
counterpart is everything that has to be decided **before** a shape can be
computed or written:

1. **Metadata keys and hparams rules** — `load_arch_hparams`
   (`qwen4exp.cpp:5-107`) reads 27 GGUF keys with three different requirement
   classes, four arity classes, and an order-sensitive PLE validation. None of
   that existed in Goose; `q4_shape` receives already-validated integers.
2. **Tensor mapping** — `q4_shape` answers "shape of id N", but nothing mapped
   id N to a GGUF name, to the layer index arity, to the declared suffix, or to
   the loader flags (required / not-required / duplicated / read-lazy /
   skip-if-virtual). Without the map the id space cannot be joined to a file.
3. **Expert and token-mixer layout** — the two presence probes
   (`create_tensor_gate_up_exps`, `create_tensor_qkv`) decide between a fused
   and a split tensor set *from the file*, not from the metadata, so they were
   outside both `q4_shape` and the hparams path.
4. **Layer rules derived from metadata** — the recurrent pattern is derived from
   `full_attention_interval` (default 4) when `attention.recurrent_layers` is
   absent, and the derived flag differs from `q4_kind` for MTP layers.
5. **Model type** — `n_layer() == 48` selects `LLM_TYPE_A3B`.

## 2. What this slice adds: three new Goose modules

One module per slice, unique prefixes, all free in the tree before this work
(`grep -rn "q4m_\|q4t_\|q4e_"` over `goose/` returned nothing).

| file | prefix | scope | exports |
|---|---|---|---|
| `source/q4m_metadata.goose` | `q4m_` | the 27 metadata keys, their requirement and arity classes, the PLE/HC/indexer/SSM guards, the recurrent pattern, `n_embd_out`, the model type | 17 |
| `source/q4t_tensor_map.goose` | `q4t_` | id → name pattern, layer arity, suffix, bucket, loader flags, per-kind presence | 10 |
| `source/q4e_expert_layout.goose` | `q4e_` | routed/shared expert widths and shapes, fused-vs-split probes, attention QKV widths, the head-width assert | 14 |

`q4e` imports the accepted `base/model_graph.goose` and calls `n_ff_exp`,
`n_ff_shexp` and `q_full_width` from it instead of restating them, so the expert
widths cannot drift from the family baseline.

The tensor ids are the canon ids (0..51) and the parameter vector keeps the
canon layout, so integration is a join on an existing key, not a rename.

## 3. Gate

`tests/build.sh <slice|join> [final|mutate]` builds one Goose module with
`--standalone` and links it against a C++ fixture whose expectations are an
independent transcription of the native lines named in each file header. No
native library is linked: the gate proves the port's rules, names and ordering,
not native numeric execution.

```
$ bash tests/build.sh q4m ; bash tests/build.sh q4t
$ bash tests/build.sh q4e ; bash tests/build.sh join

C1_02_Q4M  cases=15 values=222 failures=0
C1_02_Q4T  cases=13 values=528 failures=0
C1_02_Q4E  cases=12 values=28  failures=0
C1_02_JOIN cases=3  values=156 failures=0
```

43 cases / 934 values, 0 failures, at `-O1` and again at `-O0`
(`results/gate-O1.txt`, `results/gate-O0.txt`, byte-identical).

The `join` gate links the canonical `qwen4exp.goose` and the new tensor map into
one module and asserts, for all 3 layer kinds × 51 tensor ids, that
`q4_shape`'s "this kind owns it" and `q4t_present`'s "this kind declares it"
agree. That is the integration contract, checked rather than asserted.

## 4. Mutations

`tests/mutate.py <slice> [lo hi] [--jobs N]` holds 12 semantic mutations per
slice in two halves; each runs in its own temporary tree at `-O0` while the gate
stays at `-O1`.

```
q4m mutations 6/6 caught (range 0:6)   missed=[]
q4m mutations 6/6 caught (range 6:12)  missed=[]
q4t mutations 6/6 caught (range 0:6)   missed=[]
q4t mutations 6/6 caught (range 6:12)  missed=[]
q4e mutations 6/6 caught (range 0:6)   missed=[]
q4e mutations 6/6 caught (range 6:12)  missed=[]
```

36/36 caught, every catch a gate mismatch (`failures=N`), none an abort — an
abort would have meant the mutation only broke the build.

The mutation table itself found one real gap during development: the PLE
int32 head-range checks have two guards that are indistinguishable on a shared
input vector (`vocab == 0` masks an overlong `offset + vocab`), so case 9 now
runs every guard on a single head. One planned mutation (`o > INT32_MAX` →
`o >= INT32_MAX`) turned out to be semantically equivalent in the native code
and was replaced by a boundary that is observable.

## 5. Exact integration step

1. Copy the three modules into the canon tree unchanged:
   `source/q4m_metadata.goose` → `goose/model/q4m-metadata.goose`,
   `source/q4t_tensor_map.goose` → `goose/model/q4t-tensor-map.goose`,
   `source/q4e_expert_layout.goose` → `goose/model/q4e-expert-layout.goose`.
   `q4e` imports `base.model_graph`, which is already canonical.
2. Provide the two fail shims in the C glue next to the existing ones:
   `q4m_fail(int64_t)` and `q4t_fail(int64_t)`. `q4e` has no own extern — it
   resolves `fail` through `base/model_graph.goose` (`fg200g_fail`), exactly
   like the canonical `qwen4exp.goose`.
3. In the Qwen4Exp `load_arch_hparams` adapter, walk `q4m_key_count()` in order
   and read each key with `q4m_key_required` (always / optional / PLE-group) and
   `q4m_key_arity` (scalar / fixed four / `n_layer_all` / variable); then run the
   guards in the native order: `q4m_hc_error`, `q4m_indexer_error`,
   `q4m_ssm_error`, `q4m_ple_error`, then `q4m_ple_head_index_errors` and
   `q4m_ple_row_errors` over the head arrays. Finish with `q4m_embd_out`,
   `q4m_recr_flag` (falling back to `q4m_interval_default`), `q4m_type`.
4. At tensor creation, join by id: `q4t_pattern(id)` + `q4t_suffix(id)` give the
   name the loader looks for, `q4t_flags(id)` the loader flags, and
   `q4_shape(p, kind, id)` the shape. `q4t_present(kind, id)` gates declarations
   per layer kind; `q4t_present_ple_table(ple_heads)` gates the global PLE table.
5. For the fused/split decisions, probe the file and pass the result:
   `q4e_gate_up_mode(found)` / `q4e_routed_count(mode)` for the experts,
   `q4e_qkv_mode(found)` for attention; take the widths from `q4e_down_shape`,
   `q4e_gate_up_shape`, `q4e_gate_shape`, `q4e_shexp_shape`, `q4e_attn_widths`,
   `q4e_qkv_width`; keep `q4e_head_dim_ok` as the assert the native graph makes.

## 6. Compatibility

- No new external symbol beyond the two fail shims; every entry point is
  `i64` in, `i64` out, or a caller-owned slice. No allocation, no global state,
  no I/O, no libm.
- The modules are pure functions of their arguments: the same input gives the
  same output at `-O1` and `-O0` (checked, byte-identical gate output).
- The tensor id space and the parameter layout are the canon ones, so no call
  site has to change to adopt the map.
- `q4e` reuses the accepted base helpers rather than copying them, so a change
  in the family baseline propagates instead of silently diverging.

## 7. Boundaries (recorded, not closed)

1. **`T_NEXTN_HC_HEAD_*` (ids 49..51) have no entry in the pinned
   `upstream/llama.cpp/src/llama-arch.cpp`.** Their names come from
   `work/llama.cpp-integration/src/llama-arch.cpp:578-580`. The gate reports
   that group separately (case 13); the pinned-upstream group is ids 0..48.
   MTP itself is absent from the pinned `qwen4exp.cpp` (1213 lines) and present
   only in the 1555-line integration tree, so `q4_shape`'s MTP ids are inherited
   from P6-05, not re-derived here.
2. **The pinned upstream has no `load_ngram` PLE-skip path.** The integration
   tree creates `TENSOR_NOT_REQUIRED | TENSOR_SKIP` PLE tensors for non-PLE
   layers when `load_ngram` is off (`qwen4exp.cpp:322-330` of that tree). That
   branch is not modelled; it needs its own decision function and cases before
   the loader can rely on it.
3. **Rank divergence on the HC gammas.** `q4_shape` declares the HC norm gammas
   as `{n_embd, hc}` while the native `create_tensor` declares them one-
   dimensional `{hc_dim}`; the element count and the loaded bytes are identical,
   and the graph reshapes to `[hc_dim, nt]` before use, so both declarations
   load the same tensor. Left as canon has it; a fixture that compares declared
   ranks, not only extents, would need to reconcile the two.
4. **The gate is structural.** The oracle is a transcription, not a linked
   native library, and no model weights exist in the gate. This proves the
   metadata/tensor/expert decisions; it does not prove a real GGUF loads, and it
   makes no performance claim.
5. **`hc_head_*` are output-bucket tensors named `output_hc_*`.** That is what
   `llama-arch.cpp:521-523` says, and the map follows it; a converter that
   emits `hc_head_norm` would not be found. This is a converter-side contract,
   recorded here because it is the kind of name that reads as a typo.

## 8. Base and inputs

| file | SHA-256 |
|---|---|
| `source/q4m_metadata.goose` | `c5de38da03c433c0e7ecfeaf37ccd1061617c88d9ac8341f045de4cb3e4e93a6` |
| `source/q4t_tensor_map.goose` | `4e5e5595b47df0392480b286293361aafaf73a6a92c7346f1ad562a08acedbba` |
| `source/q4e_expert_layout.goose` | `c286704f15f464653ed9761d9bc9e5ffc871f93a6f120e1b620d56fae1d29cd4` |
| `source/join.goose` | `507b8f3339d99a98098c908a75903ec28375b8e275813cd1e12f21560909000a` |
| `source/qwen4exp_canon.goose` (verbatim canon) | `8d07a8eaf3f92f7288bdf4a49605662c35d26c6f0042861e873b0a9beea4f88c` |
| `source/base/model_graph.goose` (verbatim canon) | `8c83ca66a50322a20c76a13b55038b87646785d7fb7973531bdac15f70bd5fcc` |
| `tests/build.sh` | `6e35cc825118cb70814e2e51c57631a934a4f41b60e09d34537c781f8d701448` |
| `tests/mutate.py` | `1c0fd71603e72ffabef3c75ec780a3481c44ca8228df3e0498fdbfe48ca583da` |
| `tests/fixture_q4m.cpp` | `5aa986a480faa40cf3445aad1348854882e9efbf63b9bb2e82c4a188e1f95256` |
| `tests/fixture_q4t.cpp` | `a0dc1e4622d31e5056b2c2405892b18fe5fd570d110ed161da1875234ec8187a` |
| `tests/fixture_q4e.cpp` | `895a24a83068a41bec1c8291215447835cfa6166297e8f1cf3f95b1117e79e28` |
| `tests/join_check.cpp` | `79d2b0aa929958b9d7905f9a51d405e04b619e8c4c10f0380787cb1a66492a82` |
| `results/gate-O1.txt` | `7e217ab47589decc205e089fbe60f9a0f61d0c44e8cc80cc79d46a8e7fcaa2e7` |
| `results/gate-O0.txt` | `7e217ab47589decc205e089fbe60f9a0f61d0c44e8cc80cc79d46a8e7fcaa2e7` |
| `results/gate-O1-full.txt` | `038f128dbc4471ea48ab8788f3544ed877aa47b8472aefa4f459f8125eaa1ffa` |
| `results/gate-O0-full.txt` | `ab10ab879af0051ba467fe0b16e8bafe5d5fdcce4293d713454806cede6a2d7c` |
| `results/mutations.txt` | `e82a2a46f96348f25cd423376c620438012e6044183fd521a5d46322a1a7ab8b` |
| `tests/cases.md` | `cbaeb5a184c3676f83885cb8b22a161bc3491469efebaa79b2a13d828a8bea1c` |

Both canon copies were taken from `e4115311`; `git diff e4115311..2b443d45` over
`goose/model/` is empty, so they are also the current files.
