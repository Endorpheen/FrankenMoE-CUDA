# Build registry (short, canonical)

Start here after any context restart. This file is a pointer and a record, not a
duplicate: the authoritative lists stay where they are.

## Canonical build paths

| what | authoritative source | notes |
|---|---|---|
| composed Goose unit (modules -> C) | `tools/build-main-runtime.sh` | the module list in that script is the input; `docs/goose-migration/BUILD-SET.md` is its generated record (118 modules) |
| staged tree with carrier patches | `tools/stage-m1-tree.sh` | `--check-patches` dry-runs against upstream, touches nothing |
| migration accounting | `tools/migration-status.sh` | units, C++ classification, standalone capabilities |
| cumulative-baseline presence | `tools/check-cumulative-baseline.py` | P1-12 kernel, fg161_bulk, allocator bridge |

These scripts build the Goose unit and check composition. They do **not** build a
host runtime binary.

## Host runtime (the server that runs a model)

Accepted artefacts, outside the repository and never to be overwritten:

```
/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/build/stage-run/
    llama-server                 accepts --moe-cache-mib
    libllama.so.0.7.0            bridge calls include fg199..fg756
    libggml-base*.so.0.27.0      includes the fg161_bulk call
```

Source trees in this repository, measured on 2026-11-18:

| tree | complete? | characteristics |
|---|---|---|
| `vendor/flint` | yes (CMakeLists, common, ggml, include, src) | has `llama-moe-cache`, `--moe-cache-mib` in `common/arg.cpp`, no `fg*` calls in `src/`; `ggml/src/ggml-alloc.c` calls `fg161_bulk` |
| `work/llama.cpp-integration` | yes | **no** llama-moe-cache at all; do not use as the cumulative base |
| `.../runtime/reconstructed` (local store) | partial | full `src/` with moe-cache and `fg*` calls; `ggml/` has **no build files**; no `common/`, `tools/`, `include/` |

Consequence: building the host from tracked sources currently fails, because the
bridges the accepted artefacts contain (`fg161_bulk` in ggml, `fg199..fg756` in
llama) are not all present here. `fg367..fg406` and `fg756` exist under
`integration/modules/EXP-*-<id>-*/source/host-qualified.c`. Measured with
`tools/check-build-sources.sh`, absent from the repository: `fg199, fg210, fg212,
fg230, fg237, fg244, fg402, fg403, fg404, fg405, fg406, fg756` and `fg161`.

## Combinations measured (do not re-test blindly)

| src | ggml | include | result |
|---|---|---|---|
| reconstructed | reconstructed | - | no ggml build files |
| reconstructed | work | work | `llama_embd` undeclared |
| reconstructed | work | flint | `ggml_dsv4_hc_pre_gated`, `ggml_prec_set_acc` undeclared |
| reconstructed | flint | flint | compiles, then fails to link: six `fg*` bridges absent |
| flint | flint | flint | **builds** `libllama.so.0.7.0` and `libggml-base.so.0`; loads in the accepted server until `fg161_bulk` is unresolved |

## Pre-flight

Run `bash tools/check-build-sources.sh` before any long build. It checks the
files and symbols this registry names and stops with a specific message instead of
letting a 20-minute compile discover the problem.

## Last accepted checkpoint

`candidate-run2` (see `builds/` and the checkpoint history in the ledger). Apex
reference results: prefill +0.4 %, decode -0.7 %, VRAM -1.2 % versus EXP906.
Do not compare numbers produced by a build whose composition differs from this
registry; those are different runtimes.
