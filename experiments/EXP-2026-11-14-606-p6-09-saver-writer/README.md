# EXP P6-09 S4 — model saver writer path

Goose port of the writer half of `src/llama-model-saver.cpp`: the
`(arch, gguf_context)` constructor, the `add_kv` scalar and container
overloads with the per-layer reduction, `add_tensor` with the native
duplicate/rope rule, and `save(FILE *)` as a buffer writer. No model is
involved: every value, tensor image and layer count arrives as a parameter.

Base commit: `e6e4f469`. Reference source: `upstream/llama.cpp/src/llama-model-saver.cpp`
(writer path lines 46-155 and 490-496; byte-identical to the linked
`/home/end0/AI/LlamaServer/llama.cpp-flint-cuda` tree, whose difference is
confined to `supports_arch`, `add_kv_from_model` and
`add_tensors_from_model`). The experiment id is assigned by the lead at
integration; this tree is not committed.

## What this slice covers

`source/model-saver.goose` carries the accepted GGUF writer, tensors and
metadata context (P7-02 S1 + S2 + S3, verbatim bodies under `core_*` internal
names) plus the S4 saver block:

```
sav_init(kv state..., tst, alignment, n_layer)          ctor (36)
sav_add_u32 / i32 / u64 / f32 / bool / str / char       add_kv (55-84)
sav_add_arr(kv state..., tst, key, etype, data, n, per_layer)
                                                        add_kv template (86-129)
sav_add_strs(kv state..., key, data, n)                  vector<string> (135-141)
sav_add_tensor(tst, tpool, tne, trec, name, ndims, ne, type, data)
                                                        add_tensor (143-155)
sav_save(kv state..., tst, tpool, tne, trec, out, version)
                                                        save(FILE *) (494-496)
```

Rules kept from native:

- scalar overloads pick the GGUF type of the C++ argument: u32, i32, u64,
  f32, bool, string; `bool` normalizes any non-zero value to 1 on the wire;
- the `char` overload exists only to make the native template compile and
  aborts when reached; the port refuses with `-1` and writes nothing;
- a container with `per_layer` uses the model layer count (here
  `tst[3]`, negative means "no model"), refuses a count larger than the
  container (native `GGML_ASSERT`), writes nothing when the count is zero,
  collapses to one scalar of the element type when the first `n_values`
  elements are equal, and otherwise writes a GGUF array of the element type;
- element types follow the native template branches: u8, i8, u32, bool, i32,
  u64, f32;
- `add_tensor` skips a duplicate `rope_freqs.weight`,
  `rope_factors_long.weight` or `rope_factors_short.weight` (native returns
  after its assertion) and refuses any other duplicate (native `GGML_ABORT`);
- `save` is the full file: header, KV list, descriptors, descriptor pad and
  the data section, each tensor padded to the alignment.

The metadata key name is a parameter: `LLM_KV` naming lives in `llama-arch.cpp`
and is not reimplemented here. The gate resolves every key through the native
`LLM_KV` object, so the bytes are still compared end to end.

Out of scope (S5): the model constructor (41-44), `add_kv_from_model`
(157-449), `add_tensors_from_model` (451-488) and
`llama_model_saver_supports_arch` (15-39). Those read a `llama_model`, and the
arch list of the integration tree differs from `upstream/llama.cpp`
(`CLEF`, `VLLM_DECISION2`, extra LLM_KV keys), so an offline byte gate would
compare against the wrong table.

## Layout

```
source/model-saver.goose   port (S1 + S2 + S3 carried, S4 added)
tests/build.sh             gate build and run (-O1)
tests/fixture.cpp          native saver + port in one binary
tests/mutate.py            12 mutations, two halves of 6, port built at -O0
tests/regress-s3.sh        the accepted P7-02 S3 gate against this source
tests/gate.txt             recorded gate output
tests/mutations.txt        recorded mutation results
```

## Gate

```
bash tests/build.sh
```

`build.sh` generates `build/model_saver.c/.h` with `goose --standalone`,
compiles it with `gcc-13 -O1`, builds `tests/fixture.cpp` against the native
`libllama` (which exports `llama_model_saver` and `LLM_KV`) and `libggml-base`,
and runs the comparison. Generated C and objects stay in `build/`.

Recorded result:

```
FG_S4 cases=29 checks=154 failures=0
```

25 cases drive both implementations with the same calls and compare:

- the canonical KV dump (key, type, array element type, count, bytes);
- the tensor descriptor dump (name, type, `ggml_nbytes`, offset, `ne[0..3]`);
- the full bytes produced by `save(FILE *)` and `sav_save`;
- the port bytes reloaded through `gguf_init_from_file` (`no_alloc=true`),
  comparing counts, KV dump and descriptor dump again.

Per-layer cases need a model natively (`GGML_ASSERT(model != nullptr ||
!per_layer)`), so they use the model constructor with a `TestModel` that only
sets `arch` and `hparams.n_layer_all`; the other cases use the arch
constructor. `TestModel` stubs the seven pure virtuals of the abstract
`llama_model` exactly like the accepted P6-09 S1 fixture.

| case | content |
|------|---------|
| `u32_block`, `i32_negative`, `u64_large`, `f32_scalars` | scalar type tags, sign extension, 64-bit values, IEEE bits |
| `bool_values` | `true`, `false`, and raw 7 (native/port must both write 1) |
| `str_utf8` | UTF-8, empty and plain strings |
| `overwrite_key` | a repeated key moves to the back with the new value |
| `arr_u32_mixed`, `arr_f32`, `arr_u64` | numeric containers, no per-layer |
| `arr_empty`, `empty_file` | empty container writes no KV; a context with no KV at all |
| `strs_tokens`, `strs_long` | string arrays with empty/multibyte/long elements |
| `perlayer_same_u32`, `perlayer_same_u64` | equal prefix collapses to a scalar |
| `perlayer_diff_u32`, `perlayer_diff_f32` | differing values become an array |
| `perlayer_first_n` | a 5-element container with 3 layers writes 3 values |
| `perlayer_prefix_same` | the first 3 of `{5,5,5,9}` collapse; the 4th is ignored |
| `tensor_one`, `tensor_many`, `tensor_long_name` | one/several tensors, f32/f16/q8_0/i32, 63-byte name |
| `tensor_rope_dup` | the second rope tensor is skipped, its bytes do not appear |
| `combined` | per-layer metadata, scalars, strings and two tensors in one file |

Port-only refusals (`native aborts or asserts, so they cannot share the
process`): `char_overload` (`-1`), `tensor_dup_refused` (second non-rope
duplicate, `-1`, one tensor stays), `perlayer_no_model` (arch constructor,
`-1`), `perlayer_short` (8 layers, 4-element container, `-1`). Every refusal
also checks that the KV/tensor counts did not change.

## Mutations

```
python3 tests/mutate.py --half 1   # 6/6 caught
python3 tests/mutate.py --half 2   # 6/6 caught
```

Each mutation rewrites the port, regenerates C at `-O0`, rebuilds the fixture
and must make the gate fail. Mutant sources and binaries live in `tests/mut/`.

Half 1 (scalar overloads): u32 tagged as i32, i32 tagged as u64, bool stored
raw, u64 tagged as u32, f32 tagged as u64, string value replaced by the key.

Half 2 (containers, tensors, save): per-layer count ignores the model layer
count, every per-layer container collapses, arrays written as u64, array count
taken as byte length, duplicate rope tensor re-added, `save` writes metadata
only.

## Recorded limits

- **Key naming is native.** `LLM_KV::operator()` and the metadata key table stay
  in `llama-arch.cpp`; the port receives the resolved name. A wrong key table is
  outside this slice and cannot be caught by this gate.
- **`supports_arch` is not ported.** It belongs to the model path (S5) and the
  linked integration tree has a different excluded set than
  `upstream/llama.cpp`; gating one table against the other would be misleading.
- **Only three container instantiations are gated.** The native template is
  defined in the `.cpp` and exported for `vector<uint32_t>`, `vector<float>`,
  `vector<uint64_t>` and `vector<string>` only. The u8/i8/i32/bool branches are
  port code with no exported native counterpart to compare against; they follow
  the same native switch.
- **Per-layer element equality is byte equality.** Native compares the values
  themselves, so `+0.0`/`-0.0` and NaN payloads are the only disagreement
  points. The gate uses ordinary values.
- **`f32` takes raw IEEE-754 bits.** The port has no float bitcast at the ABI;
  the fixture passes the same 4-byte image that a native `float` argument
  carries.
- **No model and no weights.** Tensors are synthetic and materialized: the port
  has no backend, so a tensor that lives in a GPU/backend buffer must be copied
  out first. Contiguous tensors only, inherited from S3.
- **One writer produces the whole file.** `only_meta` is not exposed by
  `sav_save`; the gate compares the full bytes only, and S3 already covers the
  metadata prefix.
- **`char` and duplicate refusals replace aborts.** Native `GGML_ABORT` cannot
  run in the same process, so those branches are checked on the port side.

`tests/regress-s3.sh` rebuilds the accepted P7-02 S3 fixture against this
generated object and must still report `FG_G3 cases=41 checks=307 failures=0`.
