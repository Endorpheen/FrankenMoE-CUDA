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

## Live path on the flint build (2026-11-18)

The accepted server binary runs against our flint-built library, i.e. the same
server and the same profile as the reference:

```
LD_LIBRARY_PATH=builds/flint-acc/bin <stage-run>/llama-server \
    -m Apex-35B-V1.0-Q4_K_M.gguf -c 4096 -ngl all -fit off --cpu-moe \
    --moe-cache-mib 4992 --port 9101
health: {"status":"ok"}
```

First request with the stages enabled (prompt, 8 tokens, seed 42):

```
content:  " Paris, and its population is approximately "
counters: FG171_PENALTIES, FG171_TOP_P, FG171_TYPICAL
```

The matching run with the stages disabled, launched cleanly:

```
content:  " Paris, and its population is approximately "   (identical)
counters: none
```

So the A/B on the correct base holds: same server binary, same library build,
same profile including `--moe-cache-mib 4992`, identical text, and Goose stages
executed versus not executed proven by the counters. The earlier failed attempt
was a malformed launch path, not a runtime difference.

Building `llama-server` from `vendor/flint` is blocked by the Web UI assets: the
target always downloads a distribution and fails offline
(`UI: download dist.tar.gz.sha256 ... failed`). The library itself is unaffected.

## Building the server from this tree (unlocked 2026-11-18)

Everything in `common/` - speculative decoding, chat parsing, the sampler driver - is compiled into
the **server binary**, not into `libllama`. Until now every measurement used the prebuilt server in
`stage-run`, so a change in `common/` could not be observed at all.

```
cmake -S vendor/flint -B builds/flint-acc -DHF_ENABLED=OFF -DBUILD_UI=OFF
mkdir -p builds/flint-acc/tools/ui/dist
tar -xzf <any>/.cache/ui/dist.tar.gz -C builds/flint-acc/tools/ui/dist   # prebuilt assets
cmake --build builds/flint-acc --target llama-server -j 12
```

Run it with both libraries on the path:

```
LD_LIBRARY_PATH=$PWD/builds/flint-acc/bin:$PWD/builds/flint-acc/common \
    ./builds/flint-acc/bin/llama-server -m <model> ...
```

Two things that cost time and are now recorded:

1. **The UI download fails offline** (`HTTP response code said error` for `dist.tar.gz.sha256`) and
   the build stops on it. `-DHF_ENABLED=OFF -DBUILD_UI=OFF` alone does not stop it; extracting a
   prebuilt `dist.tar.gz` into `builds/flint-acc/tools/ui/dist` satisfies the asset step instead.
   Copies exist under `/home/end0/AI/LlamaServer/*/build-cuda/tools/ui/dist.tar.gz`.
2. **The binary is small** (about 18 KB) because the work lives in `libllama-common-base.a`; a
   missing `LD_LIBRARY_PATH` entry shows up as a symbol lookup error at startup, not as a build
   failure.

Verified: the server answers a completion request and shuts the slot down cleanly.

## The cumulative-baseline gate now runs on our runs, and what it reports

`tools/p610-paired.sh` copies the three libraries it measured into the run directory and writes
`SHA256SUMS`, `argv.txt`, `gpu.txt` and `meminfo.txt`. Before that the run directories held only JSON
and logs, so `check-cumulative-baseline.py` could not be applied to them at all - the gate was being
satisfied by habit rather than by command.

Result on `results/p610-prov-1222`:

```
ok P1-12 Goose Q4_K tiled kernel dispatch
ok P1-12 Goose kernel reference
ok P4-13 Goose allocator primitive
MISSING Goose kernel object libmoe_mmid_tiled.so
CUMULATIVE-BASELINE FAIL
```

So the three accepted Goose components **are** present in this build, and the only failure is an
artefact that this tree does not produce: `libmoe_mmid_tiled.so` belongs to the CUDA MoE acceptance
line, and nothing under `builds/` or `runtime/` contains it (only its name is referenced, by the check
itself).

Consequences, stated rather than papered over:

- a sampler-chain run cannot claim "cumulative baseline PASS" today, because the gate includes a
  component from a different line of work;
- the honest claim is narrower and true: *the three Goose components are present and identified in the
  measured binary*;
- either the kernel object is added to the build set, or the gate is told which components apply to a
  sampling-only measurement. That is a decision for Igor, not for me.

## Checkpoint 2026-11-19: the sampler chain, accepted

`builds/checkpoint-20261119-chain/` holds byte copies of the set that produced the acceptance
measurement, with `SHA256SUMS`, `git-rev.txt` (e6ca0d0a) and a `MANIFEST.md` naming the compiler, the
build script, the profile and the restore line.

    libllama.so.0.7.0          582b5619c0c8cc32cc4a49bcc2740034d57a718f99d3fe172177eaf4d43ce2ff
    libllama-common.so.0.7.0   13fe9949957f510e27f015be4ec062b5459114c4c7b66965f6793a41add5c554
    libllama-server-impl.so    ed1f7a8b67123c7ff9de7548efbf6c0af10054546ab27a1e13d3a50e046740c9
    libggml-base.so.0.27.0     73d0eff7bbc2875699dd3495be19bf735867bf7baf5cdecda95b68f7437b5c2d
    libggml-cpu.so.0.27.0      8dff918c63ebd67e6dd1559f122a45777995a18ddf6c2336900a46bf03d93448
    llama-server               8bb2ec2a795c523d1ff4c5e24e8a09cc4f1122f8ada900dfd35fee3b6c7e8a73

What it carries, in one line each: the three previously accepted Goose components, the nine sampler
stages, the two decision modules added this session, and a chain addressed by exported names with no
generated indexes. Evidence: 39 000+ differential cases, six official runs with the baseline hash, and a
green `build-main-runtime.sh` (1184 symbols, 0 duplicates).

This is the base the next stage extends. It is not to be overwritten: a new accepted set becomes a new
directory with its own hash.

### After the checkpoint: the unit gained the doors (1190 symbols)

`check-unwired-modules.py` caught what the checkpoint commit did not: `goose/speculative/spec-doors.goose`
exports six decisions and was in the **chain** build but in no accumulated unit, so it counted as unwired.

Adding it to `MODULES` makes the unit 1190 symbols (was 1184) with 0 duplicates, typechecked green. The
checkpoint's **libraries are unchanged** - the chain build already carried the doors, which is why its
symbol table has `fg_sp_*` - so the recorded hashes still describe the measured artefacts. What changed is
only the composition record of the accumulated unit.

Worth noting for the next checkpoint: the symbol count in a manifest should be taken from the build that
produced the libraries, not from a different unit, or it will drift exactly like this.

### The checkpoint restores and reproduces its hash

Started from `builds/checkpoint-20261119-chain/` alone - no path into the build tree - and measured through
`apex-test.sh`: `sha c2ff42cc1219`, decode 45.28-45.47, prefill 2257-2266. The recorded acceptance hash
reproduces from the saved bytes, which is what makes the checkpoint a restore point rather than a directory
of copies.

### Opt-in diagnostics after the checkpoint

`llama-vocab.cpp` gained a BPE-comparator cross-check and `sampling.cpp` a prefix cross-check. Both are
verification only; the BPE one is behind `FG_TK_DIAG=1` because it costs a call per merge (4156 per prompt).
Re-measured through `apex-test.sh` after gating: `sha c2ff42cc1219`, decode 44.95-46.17, prefill 2262-2287.

The working tree therefore differs in bytes from `builds/checkpoint-20261119-chain` while producing the same
accepted behaviour. The checkpoint stays the immutable evidence; the tree moves forward.
