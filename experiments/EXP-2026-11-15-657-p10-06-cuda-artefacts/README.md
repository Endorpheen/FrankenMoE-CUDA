# EXP-2026-11-15-660 — P10-06: CUDA artefact policy (arch list, nvcc flags, install layout)

Status: **ACCEPTED candidate** (model-free, GPU-free, differential). The lead's
acceptance is the commit gate. Base commit: `7bc37253`.

Work was done outside the repository, in
`/tmp/opencode/sub-660/exp/`. The shared tree, the production runtime, the CUDA
toolkit and other agents' files were not touched. No GPU, no model, no server,
no benchmark and no `nvcc` invocation was started.

## Hypothesis

The part of the CUDA artefact generation that decides *what* gets built can be
ported to Goose and pinned without a GPU: the architecture list and its
`-real`/`-virtual` semantics, the `nvcc` flag and define composition, and the
install/dist layout of the backend library. Those decisions live in the vendored
`ggml` CMake text, not in device code, so three native surfaces are enough to
gate them: `cmake -P` for the arch blocks, a real `cmake` configure for the
flags/defines/link set, and a real `cmake --install` for the file layout.

The hypothesis held: three independent slices, 124 compared cases at `-O1` and
`-O0` with 0 failures, and 36/36 mutations caught.

## Native inputs

`upstream/` is gitignored in this repository, so the native inputs are the
vendored llama.cpp checkout, verified clean for `ggml/`:

| file | role | SHA-256 |
|---|---|---|
| `upstream/llama.cpp/ggml/src/ggml-cuda/CMakeLists.txt` | arch list, suffix table, flags, defines, link set | `893edd71a30e6990b88ff86a916fa7e5edbbb7a572d942a05a7fc083706c63e7` |
| `upstream/llama.cpp/ggml/src/CMakeLists.txt` | `ggml_add_backend_library()`, `GGML_BACKEND_DIR` guard | `0e5783b891796c122f6bfcd6ee107e18fa60eceb9c04433b6c44f8096fd2acf2` |
| `upstream/llama.cpp/ggml/CMakeLists.txt` | `GGML_VERSION*` | `3af6c5ff1fdb622ef4ab4fe3f630995e0785e5967012694b98432952b4aabbab` |

Vendored revision: `0b5be7e4a25862bc2777d0c47eae18788a8c963a` (the pin recorded
in `UPSTREAMS.md`), `git status --porcelain ggml/` empty. Byte-identical copies
under `tests/native/` are the only files the oracles read.

## Inventory

Decisions examined, all verifiable without a GPU:

| native location | decision | slice |
|---|---|---|
| `ggml-cuda/CMakeLists.txt:8-57` | default `CMAKE_CUDA_ARCHITECTURES` keyed by `GGML_NATIVE`, `CUDAToolkit_VERSION`, `CMAKE_VERSION` | S1 `arch_` |
| `ggml-cuda/CMakeLists.txt:75-92` | `12X` -> `12Xa` rewrite over both the arch list and the probed native list, and the empty-entry normalisation the unquoted `set()` performs | S1 |
| `ggml-cuda/CMakeLists.txt:94-99` | "native" replaced by the probed list only when it matches the list regex | S1 |
| `ggml-cuda/CMakeLists.txt:21-23` | `-real` = device code, `-virtual` = PTX, no suffix = both | S1 |
| `ggml-cuda/CMakeLists.txt:10-19` | arch table (50 .. 120, 121), i.e. the sm number of a token | S1 |
| `ggml-cuda/CMakeLists.txt:131-156,185-188,198-201` | define list and its native order | S2 `tgt_` |
| `ggml-cuda/CMakeLists.txt:157-193` | static/shared link set, `cublasLt` by version, VMM driver, NCCL | S2 |
| `ggml-cuda/CMakeLists.txt:195-266` | `-use_fast_math -extended-lambda`, `-lineinfo`, `-compress-mode`, `-Werror all-warnings`, the joined `-Xcompiler` argument, MSVC branch | S2 |
| `ggml/src/CMakeLists.txt:379-424` | library kind, install destination, `GGML_BACKEND_*` defines, `VERSION`/`SOVERSION` and the Apple module exception, `GGML_AVAILABLE_BACKENDS` | S3 `inst_` |
| `ggml/src/CMakeLists.txt:366-371` | `GGML_BACKEND_DIR` without `GGML_BACKEND_DL` is fatal | S3 |
| `ggml/CMakeLists.txt:6-9,31` | `GGML_VERSION` / `GGML_VERSION_MAJOR` | S3 |

Rejected candidates: `tools/package-main-runtime.sh` and `scripts/make-release.sh`
(already owned by P10-05 S1, and the unit package has no CUDA-specific rule);
`ggml-cuda/*.cu` template instancing and `GGML_CUDA_FA_ALL_QUANTS` source lists
(they need the CUDA compiler to be meaningful and the file list is a glob, not a
decision); `ggml-cuda-CMakeLists.txt:59-100` (`enable_language(CUDA)` and the
`message()` lines, side effects rather than decisions); the fatbin/PTX artefact
bytes themselves (`nvcc` output, out of scope without a GPU build).

## Slices and interfaces

One Goose module per slice. Every export takes and returns scalars, `u8`/`i64`
slices only, per the flat export ABI; text goes to caller-sized buffers and the
return value is the exact byte count. Lists are the CMake list form: entries
joined with `;` in native order, no spaces. No allocation, no global state, no
I/O, no `extern` symbols.

### S1 `arch_` — `source/cuda-arch-policy.goose` (372 lines)

| export | meaning |
|---|---|
| `arch_default(cuda_major, cuda_minor, native, cmake_major, cmake_minor, out)` | the default list, or `native` |
| `arch_rewrite(list, out)` | the `12X` -> `12Xa` pass with empty entries dropped |
| `arch_resolve(arch, native_value, out)` | "native" adoption under the list regex |
| `arch_valid(text, list_mode)` | the list regex (`1`) or the single-element form (`0`) |
| `arch_kind(token)` / `arch_sm(token)` / `arch_num(token)` | suffix table, sm number, leading decimal run |
| `arch_has_ptx(token)` / `arch_has_device(token)` | derived: does the token emit PTX / device code |
| `arch_ver_ge(major, minor, want_major, want_minor)` | `VERSION_GREATER_EQUAL` on a major/minor pair |

### S2 `tgt_` — `source/cuda-target-flags.goose` (224 lines)

| export | meaning |
|---|---|
| `tgt_flags(major, minor, debug, fatal, mode, out)` | `CUDA_FLAGS` before `-Xcompiler` |
| `tgt_final(..., msvc, all_warn, host_flags, out)` | the full `CUDA_FLAGS` including the single joined `-Xcompiler` element |
| `tgt_defs(peer, graphs, mmq, cublas, novmm, fa, nopeer, nccl, nccl_found, debug, out)` | the directory define list in native order |
| `tgt_link(is_static, win, major, minor, novmm, nccl_found, cub, out)` | the link set in native order |

### S3 `inst_` — `source/cuda-install-layout.goose` (274 lines)

| export | meaning |
|---|---|
| `inst_kind(dl, shared)` | 0 STATIC, 1 SHARED, 2 MODULE |
| `inst_dest(dl, dir_set)` | 0 libdir, 1 bindir, 2 `GGML_BACKEND_DIR` |
| `inst_versioned(dl, apple)` | whether `VERSION`/`SOVERSION` are set |
| `inst_files(dl, shared, dir, name, version, soversion, out)` | installed relative paths, Linux layout |
| `inst_defs(dl, shared, out)` / `inst_public_defs(shared, out)` | target and interface define lists |
| `inst_avail(existing, backend, out)` | `GGML_AVAILABLE_BACKENDS` after one backend is added |
| `inst_version(major, minor, patch, out)` / `inst_soversion(major, out)` | `0.22.0` / `0` |
| `inst_guard(dl, dir_set)` | 1 = the native configure is fatal |

## Oracles

The oracles execute the native text; none of them re-types a rule.

- `tests/oracle_arch.py` — extracts the three arch blocks from the copied
  `ggml-cuda/CMakeLists.txt` by keyword nesting, runs them with `cmake -P`
  (`-DCUDAToolkit_VERSION=…`, `-DGGML_NATIVE=…`, `CMAKE_VERSION` overridden for
  the pre-3.24 branch) and reads the resulting list back. `native_ok`/`valid`
  apply the file's own list regex in `cmake -P`. `kind` parses the `-real` /
  `-virtual` / no-suffix comment table; `sm` parses the `# 89 == RTX 4000`
  table. The list-shape semantics were verified against CMake itself: a lone
  `;` is legal, an empty entry is dropped by the unquoted `set()`.
- `tests/oracle_tgt.py` — writes a synthetic CMake project that inlines the
  three native sections in source order, configures it (no CUDA language, no
  `nvcc`), and reads `COMPILE_OPTIONS`, `COMPILE_DEFINITIONS` and
  `LINK_LIBRARIES` back. The host-compiler branch is made deterministic by a
  `ggml_get_flags()` shim and a fake `CMAKE_CUDA_COMPILER` that prints a fixed
  version; `GGML_CUDA_CUB_3DOT2`, `GGML_STATIC` and NCCL are exercised through
  real branches (a `FindNCCL.cmake` stub). The `-Xcompiler` cut point for the
  pre-`-Xcompiler` comparison is the literal element the native text appends,
  read from that text.
- `tests/oracle_inst.py` — builds and installs a synthetic project that inlines
  the native `ggml_add_backend_library()` and `GGML_BACKEND_DIR` guard, then
  lists the real install prefix, reads the target properties, and evaluates the
  `GGML_VERSION` block with `cmake -P`.

`tests/cases/*.tsv` are the single input of both arms; lines starting with `#`
are not rows. The fixtures compare integers directly and text as raw bytes.

## Gate

```
bash tests/build.sh arch|tgt|inst [final|mutate]
```

| slice | cases | values | failures at -O1 | failures at -O0 |
|---|---:|---:|---:|---:|
| `arch_` | 60 | 60 | 0 | 0 |
| `tgt_` | 34 | 34 | 0 | 0 |
| `inst_` | 30 | 30 | 0 | 0 |

Logs: `results/gate-o1.log`, `results/gate-o0.log` (identical gate lines).

Coverage in brief: the default list at nine version/native combinations
including both sides of 11.6, 11.8, 12.7/12.8, 12.9 and 13.0 and the pre-3.24
CMake branch; the rewrite of `120`, `120-real`, `121-virtual`, `129`, `119`,
`1200`, `120a-real`, an already-rewritten token, a mixed list and the empty list;
native adoption for a valid probe, a multi-entry probe, the "No CUDA devices
found.-real" garbage, a leading empty entry and an empty probe; the list regex
with a trailing separator, a doubled separator, a leading separator and a
non-numeric token; the suffix table on `89` and `120a`; the sm table on eight
tokens. The flag list at CUDA 11.8, 12.7, 12.8, 12.9 and 13.0 with debug, fatal
warnings and four compression modes; the host-flag join with and without
`GGML_ALL_WARNINGS`, with and without MSVC; ten define combinations including
the NCCL found/not-found pair and an empty peer value; the link set for
static/shared, Windows/Linux, CUDA 10.0/10.1/12.8, VMM on/off, NCCL and CCCL.
The install layout for all four DL/shared combinations, the three destinations,
five file sets (versioned shared, static, module in bindir, module in
`GGML_BACKEND_DIR`), the Apple versioning exception, the define split, the
available-backend list with an empty, a foreign and an already-present entry,
the version strings and the `GGML_BACKEND_DIR` guard.

## Mutations

12 per slice, two halves of 6, every mutated module generated at `-O0`:

```
FG660_ARCH_MUT half=1 caught=6/6      FG660_ARCH_MUT half=2 caught=6/6
FG660_TGT_MUT  half=1 caught=6/6      FG660_TGT_MUT  half=2 caught=6/6
FG660_INST_MUT half=1 caught=6/6      FG660_INST_MUT half=2 caught=6/6
```

`results/mutate-1.log` (halves 1, 18/18) and `results/mutate-2.log` (halves 2,
18/18). One survivor was found and closed on the first pass: no default-list
case covered CUDA 12.7, so moving the 12.8 threshold to 12.7 passed; a
`default 12 7` row was added and the mutation is now caught (`failures=1`). It is
recorded here rather than hidden.

## Integration step

The three modules fit the single Goose runtime without a rewrite. Verified by
composition, not by inspection:

1. Copy the three files to `goose/misc/`:
   `cuda-arch-policy.goose`, `cuda-target-flags.goose`,
   `cuda-install-layout.goose`.
2. Add them to `MODULES` in `tools/build-main-runtime.sh` (107 modules today).
3. Re-run `bash tools/build-main-runtime.sh --repro` and
   `bash tools/package-main-runtime.sh`.

The composed unit was built outside the repository at this commit with the 107
accepted modules plus these three: `typechecked ok` (3756 specializations),
18 625 369 bytes of C, a 4 777 536-byte `-O1` object, 998 global symbols, **0
duplicate globals**, 99 extern entry points. Prefixes `arch_`/`tgt_`/`inst_` and
the internal `arc_`/`ins_`/`TGT_`/`INS_` do not collide with any accepted module,
so no `compose-goose.py` `RENAMES` entry is needed. The three modules together
also check and compile as a 137-specialization unit.

The consumer wiring is the remaining P10-06 tail: the host that emits the CUDA
artefact feeds the CUDA version, the probed native list, the option bits and the
ggml version in, and consumes the arch list, the flag/define list, the link set
and the installed file names. Nothing in the current binary calls these exports
yet.

## Compatibility and assumptions

- The gate pins decisions, not bytes: it does not build `ggml-cuda`, so it
  cannot catch a change in the CUDA sources themselves.
- `tgt_final` takes the host flag text as an input; the content of
  `ggml_get_flags()` stays with the caller (`ggml/src/CMakeLists.txt:44-100`).
- `inst_files` describes the Linux naming (`*.so` with `VERSION`/`SOVERSION`
  symlinks, `*.a` for static). macOS dylib naming and the `APPLE`
  `CMAKE_SHARED_MODULE_SUFFIX` are not ported; only the versioning predicate is
  (`inst_versioned`).
- `arch_rewrite` reproduces the empty-entry normalisation of the native
  unquoted `set(${ARCHS} ${FIXED_ARCHS})`; it is part of the port's contract.
- `GGML_CUDA_CUB_3DOT2` is an input bit of `tgt_link` because the native text
  gates `CCCL::CCCL` on it; the `FetchContent` fetch itself is not ported.

## Boundaries

No GPU, no `nvcc`, no CUDA toolkit, no model, no server, no build of llama.cpp.
The repository was not modified: sources, fixtures, oracles, cases, logs and
this card live under `/tmp/opencode/sub-660/exp/`. Generated C, objects and gate
binaries stay in `build/` and are reproducible from the sources.

## SHA-256

Sources and gate inputs (`results/hashes.txt`):

```
6973453e9da497901c2cd9998679e5c648904748ceccffbc5a6ebbea43970c8d  source/cuda-arch-policy.goose
2e6f8c62f17ff83057e90eba59a370b05a2eff44c4da3d7b15d2fdabf343d634  source/cuda-target-flags.goose
538c99a401fdee1891b1b77005682b480ee25c01493eeb6d43a17a9ef3f1a94d  source/cuda-install-layout.goose
5c83a46db80064d37a66ee0183d09b3fa31fc3250e1e5025ec19d7c2159e91b0  tests/oracle_arch.py
b2b31839cd1e45015b89db339472ac3858c08945023e6d8ae037f95fb38654dd  tests/oracle_tgt.py
96c1b2775251306172d884011a82ed6698c96b63bbf7cb9f09136c1eb2bfd004  tests/oracle_inst.py
7a943732c3b64768a889412032e6e7ef8b52accd5ab3c7e19b19b13b56210271  tests/arch_fixture.cpp
4997c9b30954d4772ad3dda374af298265a68ada0bf24eaaa6467d95674c8bda  tests/tgt_fixture.cpp
f17ed0c90bd1f720e4ae2eb288deaa8f3590f76707179d12a3767d61b0e82088  tests/inst_fixture.cpp
2e0bd20526f98127827fc0065137ae23c74feca97449ee5949b1adb319911964  tests/cases/arch.tsv
cadf4f8cbe45823b3f87c8077db8ae3b74f296ead38ee8fa0d41972b7750930b  tests/cases/tgt.tsv
a43abd29f6d8e1d17cb012c1c83dcc313aa4ffaa7b81e59e997cdc4ac8ab71c6  tests/cases/inst.tsv
f203282ffcf632d0ec6f799bb470b08146c89bbc9dbeb3d899854fd6baaa0bfd  tests/build.sh
3df41296a33167be4d68d2371ed4fa98587f30b546c03505ca718c0b917431c5  tests/mutate.py
```
