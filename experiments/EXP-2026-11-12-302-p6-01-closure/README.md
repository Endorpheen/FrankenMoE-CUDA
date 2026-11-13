# EXP302 (record): P6-01 closure -- architecture/tensor/config representation and metadata errors

Criterion: "Every audited architecture/tensor/config entry represented; unknown/invalid metadata errors preserved; no
Apex-only hardcoding."

## Representation

`llama-arch.cpp` in the reconstructed line is **byte-identical** to the flint source: `diff` produces zero changed lines.
Architecture ids, tensor-name enumerations and layer/operation tables are therefore the same set by construction -- there is
no list to drift and nothing model-specific to hardcode.

The other files that carry this surface are additive only:

| file | added | removed |
|---|---|---|
| llama-model.cpp | 96 | 0 |
| llama-hparams.cpp | 15 | 0 |
| llama-model-loader.cpp | 98 | 13 |
| llama-arch.cpp | 0 | 0 |

The 13 removed lines in the loader are the native expressions replaced by Goosed decisions (alignment test, mmap mode
selection, duplicate-weight check, shape-reshape guard, buffer-support check); each call site keeps its own throw. None of
the added lines mentions a model name -- `grep` for apex/qwen/llama4/deepseek over the added loader lines returns nothing --
so the port answers from recorded values rather than from a built-in model.

## Unknown and invalid metadata errors, measured

Three fixtures, both lines, same command:

    bad-type      key 'general.architecture' has invalid GGUF type 99  -> failed to read key-value pairs
    bad-version   gguf_init_from_reader: failed to read header
    truncated     gguf_init_from_reader: failed to read key-value pairs

Identical in both lines, including the downstream `llama_model_load: error loading model` and
`llama_model_load_from_file_impl: failed to load model` lines. `make-invalid-gguf.py` regenerates the fixtures, which are
not committed.

## Live

The accepted run loads the real model (40 blocks, MoE experts, unused-tensor warnings and all) and prints
`FG389S_GGUF_INVENTORY`, so the inventory walk and the loader decisions are exercised on real metadata, not only on the
rejection cases above.
