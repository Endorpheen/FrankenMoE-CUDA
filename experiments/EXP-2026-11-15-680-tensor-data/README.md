# EXP-2026-11-15-680 — the standalone runtime reaches the tensor bytes

Base: `4f9e2736`. Build and smoke only. No model, no server, no GPU.

## 1. What this adds

EXP671-673 read the container, the metadata table and the tensor directory -
descriptors only. A runtime that cannot reach the bytes is not an engine. This
adds the payload: `integration/goose-standalone/tensor-data.cpp` records every
tensor the substrate reader materialises and checks its placement against the
layout arithmetic of `goose/model/gguf-offsets.goose`.

```
frankenmoe-v4 tiny-hybrid.gguf
  FG_TENSOR_DATA rc=0 tensors=65 total=116128 copied=115304 layout=0 mismatches=0
  first_size=4 last_size=8192

tiny-rr.gguf      75 tensors  total=117952 copied=116928 mismatches=0
tiny-mutant.gguf  55 tensors  total=114304 copied=113680 mismatches=0
vocab-only file    0 tensors  (no payload, not an error)
```

`copied` is the real payload: 115 304 of 116 128 bytes for `tiny-hybrid`, the
difference being the alignment padding the layout accounts for and the reader does
not write.

## 2. The check that makes this worth having

`mismatches` compares ggml's own placement of every tensor against the offset the
Goose layout computed, and it is zero on all three files. Both sides derive the
offsets independently - the reader from the container, the module from the
descriptor list - so agreement over 65-75 tensors means the two layouts are the
same layout.

The first attempt reported `mismatches=65` and was wrong: ggml reports tensor data
relative to its own context buffer while `layout()` returns absolute file
offsets, so the two differ by one constant. The comparison now uses deltas, and
the constant cancels. Recording this matters because the failure looked like a
Goose defect and was a measurement defect.

## 3. Bindings added, policy unchanged

`integration/goose-standalone/graph-bindings.cpp` supplies the primitives
`goose/cpu/goose-graph-builders.goose` and `cuda/goose-mmq-dispatch.goose`
declare. They are translation only - `fg158_new` is `ggml_new_tensor_4d`,
`fg158_set_src` assigns `src[i]`, `fg158_pred` is the comparison the module asks
for - and each one aborts loudly on an unknown handle or predicate instead of
returning a plausible zero.

No decision was moved into that file. The module still chooses every node, operand
and parameter; the binding only knows how to spell those in ggml.

## 4. Capability movement

| capability | before | now |
|---|---|---|
| GGUF container, metadata, tensor directory | yes | yes |
| tensor payload in memory | no | **yes** |
| allocator arithmetic | arithmetic only | arithmetic + verified placement |
| graph builder | op/type ids | ids + all 16 primitives bound, not yet driven |
| backend, KV, tokenizer, HTTP, model load | no | no |

## 5. What is still missing to be an engine

1. **Graph execution.** The primitives are bound; nothing builds a graph from a
   model yet. That is the next layer and it needs the backend.
2. **A backend.** No CPU or GPU allocation/graph-compute seam exists in the Goose
   runtime; the Apex path is still the C++ binary.
3. **KV buffers, tokenizer mechanics, HTTP** as recorded in EXP674.

## 6. Reproduce

```
bash tools/build-standalone-runtime.sh --check          # v1, libc only
./builds/goose-standalone/frankenmoe-v4 experiments/EXP-2026-11-12-342-p6-06-tiny-hybrid/tiny-hybrid.gguf
```

The v4 link needs the unit object, `host.o`, `host_v4.o`, `tensor-data.o`,
`graph-bindings.o` and the OFF ggml libraries.
