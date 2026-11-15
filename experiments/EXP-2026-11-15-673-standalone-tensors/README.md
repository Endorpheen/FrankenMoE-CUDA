# EXP-2026-11-15-673 — v3: the standalone runtime owns the GGUF tensor directory too

Base: `c7854643`. No model, no server, no GPU. Build and smoke only.

## 1. What this adds

v2 built the metadata table through Goose. The tensor directory was still
unread. This adds it: `s3_init_tensors` / `s3_add_tensor` from
`goose/model/gguf-tensors.goose`, over the substrate reader's tensor list.

```
frankenmoe-v3 tiny-hybrid.gguf
  init_runs=1 kv=32 tensors=65 goose_built=32 agree=yes
  tensors_added=65 tdir_n=65 align=32 ne0_sum=1022
```

## 2. Why `ne0_sum` is the line that makes this a real check

`tensors_added == n_tensor` alone would pass even if every descriptor were empty:
the module accepts a tensor name, a type and four zero extents. That is why the
host now sums the *module's own recorded* `ne[0]` values and prints it next to
the substrate's answer:

| fixture | tensors | Goose `ne0_sum` | substrate `ne0_sum` |
|---|---:|---:|---:|
| `tiny-hybrid.gguf` | 65 | 1022 | 1022 |
| `tiny-rr.gguf` | 75 | 1016 | 1016 |
| `tiny-mutant.gguf` | 55 | 1028 | 1028 |

Both sides derive the extents independently: the substrate reader parses them
from the container, the module receives them and validates `ndims`, `nbytes` and
the type. Equal sums over 65–75 tensors mean the descriptors carry real extents,
not placeholders.

## 3. What was deliberately not done

1. **No tensor data is staged.** `s3_add_tensor` accepts an empty image and
   records the descriptor, which is the shape the native reader produces before
   it allocates. Passing data would require `nbytes`-exact images; that belongs
   with the allocator, not with the directory.
2. **`ndims` is passed as 4.** The module validates the value and derives the
   wire dims from `ne[]` itself, exactly like `ggml_n_dims`, so the caller's
   number is a claim the module checks rather than a value it trusts.
3. **`align` comes from the file.** `general.alignment` is read through the
   substrate and handed to `s3_init_tensors`, which validates it.

## 4. Defect found

The first v3 passed `ne = {1,1,1,1}` for every tensor. It "worked" — every
descriptor was accepted — and was meaningless. The real extents were available
all along in `gguf_get_tensor_ne`; the placeholder came from assuming the reader
did not expose them. The `ne0_sum` column exists so that this class of hollow
pass cannot happen again quietly.

## 5. Where this leaves the runtime

| layer | owner | state |
|---|---|---|
| GGUF container parsing | substrate (ggml-base) | done |
| GGUF metadata table | Goose | done, every key |
| GGUF tensor directory | Goose | done, extents verified |
| tensor data / allocation | — | not started |
| model graph | Goose (`goose-model-graph`) | present in the unit, not driven |
| KV cache, sampler, server | Goose modules | present in the unit, not driven |
| GPU backend | — | Apex binary remains the only one |

## 6. Next

Drive `goose-model-graph` over the tensor directory the module now holds, then
add the backend seam, and only then exclude the C++ runtime from the shipped
build. The Apex reference binary is untouched and no model was started.

## 7. Reproduce

```
bash tools/build-standalone-runtime.sh --check
./builds/goose-standalone/frankenmoe-v3 experiments/EXP-2026-11-12-342-p6-06-tiny-hybrid/tiny-hybrid.gguf
```
