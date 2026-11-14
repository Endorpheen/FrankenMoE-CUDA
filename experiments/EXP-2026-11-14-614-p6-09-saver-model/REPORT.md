# REPORT — P6-09 S5 / P7-02 S5, model saver model path

Base commit: `b68cffed`
Date: 2026-11-15
Status: candidate for acceptance (lead integration pending)

## Verdict

The S5 contract is ported and gated against the native `llama_model_saver` on a
synthetic model: **33 cases, 5714 comparisons, 0 failures**, and **12/12**
semantic mutations caught in two halves of six. The accepted S4 gate still
passes on the same source: **29 cases, 154 checks, 0 failures**.

`add_kv_from_model` is fully reproduced (all 185 distinct `LLM_KV` keys in
native order, with every conditional and the exact GGUF type per value).
`add_tensors_from_model` is reproduced at the descriptor level, including the
`output`/`tok_embd` aliasing branch and pointer-identity dedup for looped
architectures. `supports_arch` matches native for all 163 arch ids.

## Numbers

| item | result |
|---|---|
| gate | `FG_S5 cases=33 checks=5714 failures=0` |
| differential cases | 30 |
| port-only contract cases | 2 (charmap refusal, zero-layer model) |
| `supports_arch` sweep | 163 arch ids |
| comparisons inside model cases | 5551 |
| mutations | 12/12 (6 + 6) |
| S4 regression on the same source | `FG_S4 cases=29 checks=154 failures=0` |
| native `add_kv_from_model` statements translated | 185 distinct keys |
| hparams slots | 152 scalars/booleans, 22 arrays |
| model tensor slots | 28 (+ `tok_embd` for the alias branch) |
| port source | 2312 lines (S5 block ≈ 1230, S1-S4 carried verbatim) |

Mutation table (each line is the first gate failure it causes):

| id | mutation | caught by |
|---|---|---|
| M01 | per-layer count ignores `n_layer_nextn` | c04_nextn_per_layer |
| M02 | u32 setter writes an i32 | c01_minimal_shared_output |
| M03 | bool setter writes a u32 | c01_minimal_shared_output |
| M04 | attr `UNUSED` maps to `TYPE_NORMAL` | c17_vocab_spm_attrs |
| M05 | only `GGML_PREC_F32` becomes 0 in the prec array | c10_prec_q8_f32 |
| M06 | empty `std::string` is still written | c19_vocab_none |
| M07 | rope scaling factor inverted | c01_minimal_shared_output |
| M08 | deepseek4 branch tests `hc_mult` with `&&` | c31_dsv4_hc_mult_collapse |
| M09 | PLE layer list takes the layers without the flag | c15_ple_group |
| M10 | 1-byte charmap accepted instead of refused | c30_charmap_refusal |
| M11 | `tok_embd` saved only when the names are equal | c01_minimal_shared_output |
| M12 | tensor aliased across layers added again | c20_layer_alias |

M08 needed a dedicated case: with equal compress ratios and `n_layer_nextn > 0`
the "wrong" per-layer path collapses to one scalar while the correct
full-array path writes one value per layer. Without c31 the mutation survived.

## Memory impact

None. The gate allocates only the synthetic model, a 1 MiB port KV pool, a 1 MiB
tensor pool and a 1 MiB scratch buffer; it runs in well under a second per case
and touches no weights.

## Natively unreachable branches

These cannot be compared against the native implementation, because native
aborts or dereferences null before producing an observable result. Two are
covered by port-only contract cases; the rest are documented as boundaries.

| branch | native behaviour | port behaviour | gate |
|---|---|---|---|
| non-empty `tokenizer.ggml.precompiled_charsmap` | `add_kv(char)` -> `GGML_ABORT` | returns `-1`, writes nothing | port-only c30 |
| `n_layer_all == 0` | `hparams.n_expert_used()` -> `GGML_ABORT` | writes nothing per-layer, returns 0 | port-only c23 |
| duplicate non-rope tensor name (incl. duplicate global slots) | `GGML_ASSERT` in `add_tensor` | `sav_add_tensor` returns `-1`, enumeration stops | S4 gate only |
| `output != nullptr` and `tok_embd == nullptr` | dereferences `tok_embd->name` -> crash | alias branch requires both, skips | not reachable |
| `n_layer_all > LLAMA_MAX_LAYERS` for a per-layer array | template assert `n_values <= value.size()` | `sav_add_arr` refuses (`nl > n`) | not reachable |
| `arc == DEEPSEEK4` with `n_layer_all` larger than the ratio array | out-of-range vector construction | n/a (tables are host-supplied) | not reachable |

`supports_arch` is not a branch of the saver body, but it is compared over every
enum value. The excluded ids are passed by the host so the tree's enum numbering
stays out of the port.

## What is not claimed

- This is not a byte-exact saved-model gate: only KV records and tensor
  descriptors are compared, and the model is synthetic.
- The vocabulary is real (loaded by the native loader from a tiny GGUF), but it
  is 3-16 tokens; no tokenizer post-processing beyond the loader is exercised.
- Key names and the arch/rope strings are resolved natively and passed in, as in
  the accepted S4 gate.
- The precision map order is pointer order by construction; the gate mirrors the
  native container instead of inventing an order.

## SHA-256

```
298c1f2a36ebe900dd48a9a4293b148d8a53e28690b85629f17ba423a1e18632  source/model-saver.goose
083c01f6a1f20785a7c3aa358080680c2306bb3ddfac5339bdedab56fba29b18  source/s4-carry.goose
a2b709c6c3ffaba51a1eaba37b69fe2a2e1a7f44dd753d511c266b812c7d155a  tests/gen.py
5478e7b84ec8672bd0c465f10b45089760e9529af427199b309091e5b32cb4e7  tests/slots.h
af0b58f0dda6b92747806ec1ddd99ada2baf13892ba80c199638b8da410d24e7  tests/fixture.cpp
39931dfad8081188ede0711f1573e3f53bbc0bc14389ea671bac83a87622eda6  tests/build.sh
e3b89f996a1547f32de0cfbb5796d8113e909d72ba54d848b07c14fe280f388a  tests/mutate.py
039a0a4f898539e5ad7be1b20cc901d068702171e6d7c1a5029296414d56e8f5  tests/gate.txt
03de3d967b8487490fb7b16bd348bc7607f2a969980e594822489e6fd339cbf0  tests/mutations.txt
e6499c00b04c0943b53946121d06f453daa91eec0375d1b0a60e7621f67886f0  tests/regress-s4.sh
72a7c7f7b78d84f255221d764e6a44d6ae02340806087a12ca8089577f08aef7  tests/regress-s4.txt
```

`source/s4-carry.goose` is byte-identical to the accepted
`goose/model/model-saver.goose` at `b68cffed`, so the integration step is a
single-file replacement.

## Integration step

1. Copy `source/model-saver.goose` to `goose/model/model-saver.goose`
   (replaces the S4 file; S1-S4 bodies are carried verbatim, S5 is new).
2. Copy `tests/` into `experiments/EXP-<id>-p6-09-saver-model/` next to
   `source/`, run `bash tests/build.sh`, `tests/mutate.py` halves and
   `tests/regress-s4.sh`, and re-record the three outputs.
3. Update `docs/goose-migration/P6-09-COVERAGE.md` and
   `docs/goose-migration/P7-02-COVERAGE.md`: S5 -> done (EXP<id>), reopen the
   boundary lists (no weights, synthetic model).
4. Update `docs/goose-migration/tasks.json` for P6-09 and P7-02: slices done,
   `remaining` empty for S5, `coverage_doc` unchanged, `updated_at` refreshed.
