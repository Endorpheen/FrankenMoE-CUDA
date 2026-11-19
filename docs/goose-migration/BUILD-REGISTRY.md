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

`candidate-run2` (see `builds/` and the checkpoint history in the ledger).

**Correction (EXP-2026-11-19-1298).** The earlier statement here - "prefill +0.4 %, decode -0.7 %,
VRAM -1.2 % versus EXP906" - compared two builds of the **same composition**, both compiled from
`vendor/flint/src`. Neither contains the host-registration feature that EXP906 was accepted for, so the
comparison established parity between two builds that had equally lost it. It is exactly the pattern
that must not be presented as evidence: two equally slow builds agreeing with each other.

## Missing accepted component: host registration (debt, opened 2026-11-19)

Measured on identical conditions (same profile, same 32768/512 request, same argv, port 9101):

```
reference binary (EXP906, built from runtime/reconstructed)   decode 55.52  prefill 2625
candidate builds/flint-acc (compiled from vendor/flint)       decode ~45.2  prefill ~2260
  strings <lib> | grep "mapped expert ranges"    reference: 1   candidate: 0
```

`tools/check-accepted-components.py` reports it as the one absent accepted component; every other entry
(moe-cache, the sampler chain, the tiled Q4_K bridge, the shim marker) is present.

### What exists already, and where

| piece | file |
|---|---|
| `llama_model::pin_mmap_experts(ggml_backend_reg_t)` (~80 lines) | `runtime/reconstructed/src/llama-model.cpp:1300` |
| `llama_mmap_expert_pin::owner` - acquire, release, merge_pages, fits_budget | `runtime/reconstructed/src/llama-mmap-expert-pin.h` (81 lines) |
| the call site, guarded by `props.caps.host_buffer` | `runtime/reconstructed/src/llama-context.cpp:582` |
| members `expert_pin`, `expert_pin_mutex`, `regular_mmap` | same file, impl struct at ~1295 |

**Nothing is to be re-implemented.** The work is to connect these to the cumulative build, and the
obstacle is that the two trees diverged: the reconstructed file is a `llama_model` method while the
vendored file has `llama_model_base` and a differently shaped context initialiser with the MoE cache.
The port therefore needs its call site re-established against the vendored context, not a copy-paste.

### Connection plan (5 steps, each verifiable)

1. copy `llama-mmap-expert-pin.h` into `vendor/flint/src/` - pure addition, no behaviour until used;
2. add `regular_mmap`, `expert_pin`, `expert_pin_mutex` to the vendored impl struct and set
   `regular_mmap` where the mmap path is taken;
3. add `pin_mmap_experts` to the vendored model class, body unchanged;
4. call it from the vendored context initialiser under the same `host_buffer` capability guard;
5. `python3 tools/check-accepted-components.py` must report `host-registration` present, then an
   acceptance run must show the decode gain, then this entry is replaced by the numbers.

Until step 5 passes, this stays open debt and the migration continues; the rule not to stop for
optimisation is respected, and the loss is not hidden.

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

## Gate suite after the sampler work (2026-11-19)

| check | result |
|---|---|
| `check-goose-modules.py` | ok |
| `check-unwired-modules.py` | PASS (no module outside the recorded baseline) |
| `check-generated-files.py` | ok, no generated file staged |
| `check-build-sources-tracked.py` | ok, all tracked |
| `check-goose-symbols.py` | 2625 symbols present, none missing |
| `check-cumulative-baseline.py` | **PASS** once the kernel object is snapshotted (see the correction in EXP-1223) |
| `check-goose-tree.py` | 20 warnings, 5 provenance drifts |

### The five provenance drifts, named

```
model/bpe-tokenize.goose
model/gguf-reader.goose
model/qwen35moe.goose
server/cors-policy.goose
server/api-key-policy.goose
```

Each is a canonical module that moved ahead of the experiment copy recorded for it. Four are **pre-existing** -
their last commits are from earlier sessions - and I synced only the one I changed today
(`sampling/sampler-chain-full.goose`, which no longer appears). Syncing the others would require re-deriving
which commit of each module its recorded source is meant to represent, and guessing that would defeat the
check. Left for the owner with the names, rather than silenced.

### The two checks that take a directory, now run

```
check-symbol-callers.py builds/flint-acc/bin/libllama.so.0   rc=0, 2369 call targets
check-dead-objects.py <flat dir of objects>                  PASS, 349 objects, 0 wholly dead,
                                                             0 partly dead
```

`check-dead-objects.py` lists `.o` files **directly** in the directory it is given, and this build nests them
under `CMakeFiles/`, so it reported "no objects" twice before I collected them flat into a temporary directory.
That is a limitation of the invocation, not a finding: with the objects present it reports that every linked
bridge object has at least one caller.

Running it also settles a question the earlier "no objects" answer could have hidden: the bridge objects this
tool hunts **do** exist in the build (the EXP-271/273/275 ports compile their `bridge.c` into the server), and
none of them is dead.

## Checkpoint 2026-11-19-arch: the recipe registry wired

`builds/checkpoint-20261119-arch/` holds the set that carries the arch hook, with `SHA256SUMS`, `git-rev.txt`
and `MANIFEST.md`. What differs from `checkpoint-20261119-chain`:

    libllama.so.0.7.0        753268246495bf6c09282e13aceb58c0aea0012d0f4e71f4280d94fbc02048ee   (was 582b5619…)
    libllama-common.so.0.7.0 0f5ef3569c1db1e22517f02bc2e3c0d766428cd2d66d201e2c5e17e7c93f0c7c   (was 13fe9949…)

The ggml libraries, the server-impl library and the binary are byte-identical to the previous checkpoint, which is
what a change confined to two translation units should look like. Acceptance hash `c2ff42cc1219`, unchanged.


## Gate suite, complete (2026-11-19, latest)

| check | result |
|---|---|
| `check-goose-modules.py` | ok |
| `check-unwired-modules.py` | PASS |
| `check-generated-files.py` | ok |
| `check-build-sources-tracked.py` | ok |
| `check-goose-symbols.py` | 2625 present, none missing |
| `check-symbol-callers.py` | rc=0, 2369 call targets |
| `check-dead-objects.py` | PASS, 349 objects, 0 dead |
| `check-cumulative-baseline.py` | **PASS** |
| `check-unresolved-symbols.sh` | **PASS** (new: catches an extern with no provider) |

Nine of nine, with the caveat that two of them need the right invocation: `check-dead-objects.py` wants a flat
directory of objects (the build nests them under CMakeFiles), and `check-cumulative-baseline.py` wants the four
artefacts inside the run directory. Both are now produced by `tools/p610-paired.sh`, which snapshots them from
their real producers including the kernel object from the P1-12 experiment.


## The ninth check, and the class it closes

`check-unresolved-symbols.sh` reads the shipped library and fails on any undefined symbol that is not libc,
libstdc++ or the ggml/gguf families. It exists because `metrics-traces.goose` declared
`extern "mt_fmt_g6"` and nothing provided it: the module composed, typechecked and counted in the symbol total,
and the library it joined could not be loaded at all.

Every previous check looked at the build; this one looks at the artefact the run loads, which is where a promise
is either kept or discovered broken. Its own first version produced 139 false failures because I had forgotten
`GLIBCXX` versioned symbols - caught by reading the output rather than by trusting the verdict.
