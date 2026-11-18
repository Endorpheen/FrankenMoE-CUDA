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

## flint build: what it needs at link time

`cmake --build builds/flint-acc --target llama` succeeds (rc=0, six
`fg_sampler_*` symbols in the library), but linking anything against it fails
until two accepted bridge definitions are provided:

```
libggml-cpu.so.0: undefined reference to `fg_cpu_simd_mmid_tiled_q4_k`
libggml-base.so.0: undefined reference to `fg161_bulk`
```

Both are accepted components whose generated C is a build artefact, not tracked
source: the P1-12 tiled kernel and the P4-13 allocator bulk call. They must be
linked into the ggml libraries (or shipped beside them) before the library is
usable, and `tools/check-build-sources.sh` reports their absence.

Warning recorded from the same run: a failed link produced no test binary, and
`diff` on two empty outputs reported "identical". Empty output is never evidence;
check the binary exists and printed its expected number of lines first.

## flint build: working configuration (2026-11-18)

```
cmake -S vendor/flint -B builds/flint-acc -DCMAKE_BUILD_TYPE=Release -DGGML_CUDA=ON \
      -DLLAMA_CURL=OFF -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF \
      -DLLAMA_BUILD_TOOLS=OFF -DLLAMA_BUILD_APP=OFF
cmake --build builds/flint-acc --target llama -j 12
```

The two accepted bridges are linked by `vendor/flint/ggml/src/CMakeLists.txt`
from their artefacts, and the sampler chain joins the `llama` target through
`vendor/flint/src/CMakeLists.txt`. With this build the stage differentials run and
match:

```
typical: lines off/on 200/200, identical, FG171_TYPICAL goose calls=1 kept=2
top-p:   lines off/on 200/200, identical, FG171_TOP_P goose calls=1 kept=1
```

Two path mistakes were fixed on the way and are worth remembering: a CMake
`${CMAKE_SOURCE_DIR}/../...` reaches `vendor/`, not the repository root, and a
failed link leaves no binary - the test must stop then, because an empty diff
reads as "identical".

The bridge artefacts are still build outputs rather than generated by the build;
that is recorded as work, not as an accepted state.
