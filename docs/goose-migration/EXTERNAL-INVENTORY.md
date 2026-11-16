# External artefact inventory (stage 1 of the dependency transfer)

Every external artefact the build consumes, with the provenance needed to move it without guessing. Hashes are the
first sixteen hex digits of SHA-256 and are recorded so a later transfer can be checked rather than trusted.

## Root A - `/home/end0/AI/LlamaServer/llama.cpp-flint-cuda`

Provenance: a git repository, HEAD `1b44e307 "Add Flint model (FlintForCausalLM). Assisted by Codex"`. Licence
inherited from llama.cpp (MIT); must be carried into `vendor/flint/` with the source.

| artefact | count | hash | used how |
|---|---|---|---|
| `build-cuda/**/*.o` | 356 files (44 reach the link) | aggregate `1ca744ae27b5f8ef` | linked directly by `build.py` |
| `src/models/models.h` | 1 file, 3022 lines | `b6323dc9e12f8a3a` | included by `llama-model.cpp` |
| `libggml.so.0` | 1 | `1e52e0dc14ed` | linked |
| `libggml-base.so.0` | 1 | `8cce13c85f19` | linked |
| `libggml-cpu.so.0` | 1 | `b4ec8b3a9eae` | linked |
| `libggml-cuda.so.0` | 1 | `b469d1c07c76` | linked |
| `libllama-common.so.0` | 1 | `d05eba2bdb02` | linked |
| `libllama.so.0` | 1 | `b78c76356acf` | present in the flint tree; our build produces its own |

## Root B - `/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108`

Provenance: a git repository, HEAD `dd2e6f1c "Style: normalize every Goose source in the repository"`.

| artefact | count | used how |
|---|---|---|
| `runtime/reconstructed/build/*.o` | 121 files, 30 reach the link | linked directly by `build.py` |
| `experiments/*/source/*.c` | per experiment | compiled into modules and bridges, or copied by the consolidator |

## What the inventory already settles

The 356 objects and the 6 libraries are binary artefacts of a tree that is itself under git. That means the honest way to
bring them inside the repository is **not** to copy the objects: it is to move `src/` and `include/` and rebuild, so the
committed thing is the source the objects came from. Copying 356 `.o` files would pin behaviour to binaries whose source
the repository does not describe.

`models.h` at 3022 lines is the one file whose content is both consumed by our `llama-model.cpp` and modified by us, so it
must move with the flint commit recorded and the local edits separated - otherwise a future reader cannot tell which
lines are upstream and which are ours.

## Next stage

Stage 2 moves `vendor/flint/src` (headers plus `models.h`) and repoints `-I`; stage 3 rewrites the absolute paths in
`build.py` to derive from the script location; stage 4 rebuilds everything from the moved sources and only then compares
the hash, decode, prefill and VRAM against the recorded reference. The 44-object link must disappear in stage 4, not
before, or the comparison would be against a mixed build.
