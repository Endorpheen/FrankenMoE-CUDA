# EXP-2026-11-15-672 — the standalone runtime reads a real GGUF: substrate container, Goose metadata

Base: `9e87dd6a`. No model, no server, no GPU. Build and smoke only.

## 1. What this adds

EXP671 proved the composed unit links and answers with libc alone. It could not
read a model, because the GGUF container is substrate: the unit owns the metadata
*decisions*, not the file format. This experiment adds exactly that one edge and
proves it on files that exist.

```
builds/goose-standalone/frankenmoe-v2 <model.gguf>
  FG_STANDALONE_V2 init_runs=1 types=43 kv=20 tensors=0
  first_key="general.architecture" goose_built=18
  goose_first="general.architecture" agree=yes
```

Three real vocab GGUFs from the upstream tree:

| file | kv | Goose accepted | agree |
|---|---:|---:|---|
| `ggml-vocab-bert-bge.gguf` | 20 | 20 | yes |
| `ggml-vocab-gpt-neox.gguf` | 17 | 17 | yes |
| `ggml-vocab-llama-spm.gguf` | 22 | 22 | yes |
| `ggml-vocab-aquila.gguf` | 18 | 18 | yes |

Every key of every file is accepted, arrays included.

`agree=yes` means the substrate reader and the Goose table report the same first
key, byte for byte, and that Goose accepted every key it was offered.

## 2. Why the OFF substrate libraries are linked

The ON (`FG_GOOSE_*`) ggml libraries already contain the carrier bridges, and
those bridges enter `goose_init` through their own guard. Linking them next to
the standalone unit puts **two** initialisers in one process, which is the exact
invariant the design forbids (`goose_init_runs` must be 1).

The standalone runtime therefore links the OFF ggml libraries: pure substrate,
no bridges, one initialiser. This is not a workaround; it is the correct
dependency direction. A future one-binary build takes the same position, and the
carrier bridges disappear with the C++ runtime.

## 3. Three defects found and fixed while wiring it

Each is recorded because each would have shipped as a silent or loud failure:

1. **`s1_init` arity.** The export takes eight parallel slices (state, key
   offsets, key lengths, types, array markers, three value slots), not the four I
   first passed. The mismatch segfaulted inside `s1_init_g`. The unit header is
   the authority: `main.h` declares every export with its exact signature.
2. **Type-specific getters.** `gguf_get_val_u32` asserts when the value is not a
   `uint32`. Every GGUF type needs its own getter; reading a `float32` through
   the `u32` accessor aborts with `GGML_ASSERT(type == type)`.
3. **Reading the module's own field names.** `mime_text` and `abi_blck_size`
   have the contracts their modules declare (`mime_of` + `mime_text` is a
   two-step slice contract; `abi_blck_size` takes the type id), not the shape one
   would guess. EXP671's first smoke failed on both.

## 4. What this does not do yet

1. Array payloads are staged by the host: numeric arrays as 8-byte words, string
   arrays as the NUL-separated concatenation the module's contract names. The
   first attempt used a 64 KiB pool and the tokenizer arrays overflowed it
   (`index 65536 out of bounds`); the pool is now 16 MiB and the string staging
   4 MiB, sized for a real tokenizer.
2. **No tensor data.** `no_alloc = true`: the container is read, the tensor
   directory is not materialised.
3. **No model, no graph, no GPU, no HTTP.** This is still the runtime core.
4. `tensors=0` on all three files is correct: they are vocab-only fixtures.

## 5. Next, in order

1. Stage the array payload so `s1_set_arr*` can take it, and require every key
   of a file to be accepted.
2. Materialise the tensor directory and drive `goose-model-graph` over it.
3. Give the runtime a backend seam for the Apex GPU path; the existing binary
   stays the reference until the new one answers the same request.
4. Then exclude the C++ runtime from the shipped build in one change.

## 6. Reproduce

```
bash tools/build-standalone-runtime.sh --check
./builds/goose-standalone/frankenmoe-v2 builds/stage-m1/llama.cpp/models/ggml-vocab-bert-bge.gguf
```

The script writes the whole log from its first line (rule 14) and refuses a
binary that links a C++ or ggml library in the v1 build.
