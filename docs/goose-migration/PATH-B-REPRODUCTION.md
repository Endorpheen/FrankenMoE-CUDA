# Reproducing the accepted runtime (path B), from tracked sources

`BUILD-PROVENANCE-GAP.md` established that the release was produced by path B
(`tools/stage-m1-tree.sh` + `tools/build-from-tree.sh` over a consolidated tree) while a fresh run of
path A (`tools/build-independent-apex.sh` → `build.py`) gives a different artefact. This document is
what path B needs, what was checked, and the exact commands.

## Inputs, and where each comes from

| input | source | reproducible from the repository? |
|---|---|---|
| carrier sources | `runtime/reconstructed/src/*.cpp` | yes, tracked |
| carrier patches | `patches/*.diff` | yes, 11/11 apply (`stage-m1-tree.sh --check-patches`, 2026-11-19) |
| bridges, host adapter, gates | `integration/m1` | yes, tracked |
| Goose modules | selected from `experiments/*/source` by the `<prefix>_enabled` rule | **yes** - checked 2026-11-19: the external snapshot and this repository give the same 86 of 86 picks, none different, none missing |
| flint objects | compiled from `vendor/flint` | yes, but they must be **compiled**, not copied; objects are never committed |
| ggml libraries | compiled from `vendor/flint/ggml` | yes, same |
| `extra-objects` | hand-built objects from earlier experiments | **no** - these are the one genuine gap |

## What was changed for this

`tools/consolidate-runtime-tree.sh` defaulted `BASE` to
`/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108`, an external snapshot of this
repository (528 experiments against the tree's 1134). Measured before the change: for all 86 module
prefixes the selection rule picks the same file from both. The snapshot was adding nothing and made the
build depend on a directory outside the tree, so the default is now `$ROOT/experiments`. `BASE` stays
overridable for archaeology.

## The pipeline

```
# 1. libraries and flint objects, from the vendored sources
cmake -S vendor/flint -B /tmp/pathB/support -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13 \
      -DGGML_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=89 \
      -DLLAMA_BUILD_APP=OFF -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF -DLLAMA_BUILD_UI=OFF
cmake --build /tmp/pathB/support --target llama-server -j 6

# 2. the consolidated tree, with the composed unit
bash tools/consolidate-runtime-tree.sh          # modules, bridges, our-src, flint-objects, libs
bash tools/stage-m1-tree.sh /tmp/pathB/tree --with-unit

# 3. the runtime
bash tools/build-from-tree.sh /tmp/pathB/out

# 4. the gate that must pass before anything is measured
bash tools/verify-candidate.sh /tmp/pathB/out
```

Step 4 is the one that was missing all day: it compares the artefact against the accepted component
list and prints the door count, so a build that silently drops `fg_sp_prefix` or `FG_GGUF_PARSE` is
rejected before it is measured rather than after it is published.

## What is still not reproducible

`extra-objects`: hand-built objects that `consolidate-runtime-tree.sh` copies from the experiment tree.
Until they are either rebuilt from tracked sources or shown to be unnecessary, path B is reproducible
except for that directory, and a release built from it must say so in `BUILD-COMMAND`.
