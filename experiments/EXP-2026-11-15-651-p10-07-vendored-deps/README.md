# EXP-2026-11-15-649 — P10-07: replace or justify vendored executable dependencies

Status: **ACCEPTED candidate** (model-free, differential). The lead's acceptance
is the commit gate. Base commit: `70de805c`. The native canon is the pinned
`upstream/llama.cpp` at `0b5be7e4a25862bc2777d0c47eae18788a8c963a`; the shipped
tree `work/llama.cpp-integration` (fork `4aaad5d`) was read only, never touched,
and its one divergence is recorded below.

Work was done outside the repository, in
`/tmp/opencode/sub-653/EXP-2026-11-15-649-p10-07-vendored-deps/`. The repository,
the production runtime and other agents' files were not modified. No model, GPU,
server, llama.cpp build or benchmark was started, and nothing was downloaded.

## Hypothesis

P10-07 asks for every vendored helper to be classified and for every survivor to
be justified rather than assumed. The vendored C++ is not one thing: five small
projects are pure functions that can be owned or dropped, one is a transport
primitive that must stay, one is compiled into the binary and called by nothing
the product runs, one is a 6261-line template engine that already has an owned
port, and 480 lines under a directory called `vendors/` are the project's own
API shims rather than a dependency at all.

The hypothesis held, and it is checkable without a model: the decisions that
keep a vendored file admitted — the licence payload that ships with it, the pin
and patch anchor that make it reproducible, the build-flag and linkage rules that
select it — can be ported to Goose and frozen by gates that execute the native
CMake and parse the native tables on every run. Three slices, 240 compared values
at `-O1` and `-O0` with 0 failures, 12/12 mutations caught.

## Inventory

Scope: `vendor/**`, `ggml/src/ggml-cuda/vendors/**`, `common/jinja/**` and the web
UI's `src/lib/vendors/**`, at both the canon and the shipped tree. 27 files,
6,491,829 bytes under `vendor/`, plus 480 lines of backend shims, 6,261 lines of
Jinja and 37,515 lines of browser JavaScript.

| # | component | version / pin | licence | consumer | verdict |
|---|---|---|---:|---|---|
| 1 | nlohmann/json | 3.12.0 (`releases/latest`) | MIT | `common/json.cpp`, tests | replace with the owned parser/dumper |
| 2 | cpp-httplib | 0.54.1 canon, 0.53.1 shipped | MIT | server, `common/http.h` | justified: transport |
| 3 | miniaudio | 0.11.25, commit `9634bedb` | public domain / MIT-0 | `mtmd` audio input | removable with the multimodal scope |
| 4 | stb_image | 2.30, branch `master` | public domain | `mtmd` image input | removable with the multimodal scope |
| 5 | sheredom/subprocess.h | commit `0dccaa9a` | Unlicense | `common/subproc.h`, MCP, mtmd | justified: process spawn |
| 6 | xxhash | 0.8.3, commit `9f465f1e` | BSD-2-Clause | `examples/gguf-hash` only | drop from the product link |
| 7 | sha1 (clibs) | commit `e1e2536f` | public domain | `examples/gguf-hash` only | drop from the product link |
| 8 | sha256 (jb55) | commit `5e637272` | public domain | mtmd cache key, gguf-hash | justified: digest |
| 9 | rotate-bits | commit `27e78494` | MIT | transitive of sha256 | justified as part of 8 |
| 10 | Jinja engine | llama.cpp in-tree, 6,261 lines | MIT (llama.cpp) | chat templates | replace with the owned engine |
| 11 | nerdamer-prime | vendored JS | MIT | web UI sandbox global | UI asset, out of the runtime |
| 12 | big-integer | vendored JS | Unlicense | nerdamer dependency | UI asset |
| 13 | decimal.js | 10.6.0 | MIT | nerdamer dependency | UI asset |
| 14 | `ggml-cuda/vendors/*.h` | in-tree | llama.cpp | CUDA/HIP/MUSA API shims | not a dependency: owned shim |

Counts: 14 components, 13 third-party, 1 project-owned shim set; 9 replaceable
(2 with an owned replacement already accepted, 7 droppable by link or scope), 4
justified, 1 not a dependency. The full audit, including the residual role of
each survivor, is `REPORT.md`.

## The three slices

| slice | module | ported decision | export count |
|---|---|---|---|
| S1 `lic_` | `source/vendor-licence-payload.goose` | the embedded licence entry and the generated `license.cpp`: `IS_ABSOLUTE` per host, the title, the underline, the raw-string delimiter, the `file(READ)` text mode, the missing-file warning, the missing-target error | 9 |
| S2 `vnd_` | `source/vendor-provenance.goose` | the pin class of a download URL, reproducibility, `str.count()` anchor semantics, the exactly-once patch rule, and the admission state of a file on disk | 7 |
| S3 `sys_` | `source/vendor-selection.goose` | the `LLAMA_USE_SYSTEM_GGML` resolution plan, the vendor subdirectory set, the component-to-vendor linkage, and the two build mismatch masks | 6 |

Native citations are in each module header: `cmake/license.cmake:6-45`,
`scripts/sync_vendor.py:8-100`, `CMakeLists.txt:51,204-212`,
`vendor/CMakeLists.txt`, `common/CMakeLists.txt:132,147`,
`tools/mtmd/CMakeLists.txt:83`, `tools/server/CMakeLists.txt:51`.

## The oracle

No expectation is a hand-copied native table.

- `tests/oracle_lic.py` **executes** the byte-identical `cmake/license.cmake` in
  `cmake -P` script mode, one fresh script per case, and reads back the bytes the
  native functions wrote. Two script-mode accommodations are asserted, not
  assumed: `define_property()` is not scriptable, and `license_generate()` ends in
  `message(FATAL_ERROR)` when no target exists, so `message` is shadowed for that
  one case after the file has been written.
- `tests/oracle_vnd.py` parses the vendor table and the patch table out of the
  byte-identical `scripts/sync_vendor.py` (its own constants expand its
  f-strings), counts anchors with Python's `str.count()` — the function the native
  script calls — and recomputes the tree facts from the real `vendor/` directory.
- `tests/oracle_sys.py` extracts the two ggml `if()` blocks and includes
  `vendor/CMakeLists.txt` under recording stubs in `cmake -P`, so the added
  directory set and its order are CMake's answer; the component linkage is parsed
  from the real `target_link_libraries` lines.

## Gate numbers

| slice | cases | values | failures `-O1` | failures `-O0` |
|---|---:|---:|---:|---:|
| `lic_` | 60 | 60 | 0 | 0 |
| `vnd_` | 141 | 141 | 0 | 0 |
| `sys_` | 39 | 39 | 0 | 0 |
| **total** | **240** | **240** | **0** | **0** |

`results/gate-o1.log`, `results/gate-o0.log`.

What the gate already found, and what was corrected in the port before it went
green:

1. `common` also links `cpp-httplib` (`common/CMakeLists.txt:147`), so the
   component mask is 44, not 12 — the port said 12 and the gate said so.
2. `string(REGEX REPLACE "." "=" ...)` replaces newlines too, so the underline is
   exactly as long as the title; the first draft preserved the newline.
3. `file(READ)` converts CRLF to LF **and drops one CR at end of file**; the first
   draft only did the CRLF conversion.
4. `IS_ABSOLUTE` is host-dependent: on the canonical POSIX host `C:/x` is a
   relative name, so the platform is an explicit argument instead of an assumed
   rule.
5. `vendor::nlohmann` is added unconditionally but linked only by `common`; with
   `LLAMA_BUILD_COMMON=OFF` the build compiles a third-party header tree that
   nothing links, which is the first real finding of the audit.

## Mutations

`python3 tests/mutate.py 1|2` — six mutations per half, two per slice; each
rewrites one decision, regenerates that module at `-O0`, relinks the fixture and
reruns the gate. Both halves 6/6, 12/12 total. `results/mutate-1.log`,
`results/mutate-2.log`.

## Integration step

1. Copy the three modules into the canonical tree:
   `source/vendor-licence-payload.goose` → `goose/misc/vendor-licence.goose`,
   `source/vendor-provenance.goose` → `goose/misc/vendor-provenance.goose`,
   `source/vendor-selection.goose` → `goose/misc/vendor-selection.goose`.
2. Append those three paths to `MODULES` in `tools/build-main-runtime.sh`. They
   import `std` only; helper families are `lic_`, `vnd_`, `sys_` (no collision
   against the current 88-module list), so no `compose-goose.py` `RENAMES` entry
   is needed.
3. Run `bash tools/build-main-runtime.sh`: expect 0 duplicate symbols and the new
   exports in `main.h`.
4. Register the files in `goose/MANIFEST.json` through the normal sync and run
   `python3 tools/check-goose-tree.py`.
5. The audit front-end (a future `tools/check-vendored-deps.py`) calls `vnd_` and
   `sys_` instead of recomputing the decisions; the CMake files themselves stay
   as they are, because the acceptance is classification plus justification, not
   a build rewrite.

## Compatibility

- Exports are additive; no existing module, symbol, header or ABI changes.
- Every export is a pure function of its arguments: no global state, no
  allocation, no I/O, no CLOCK. Both optimisation levels and any call order give
  the same bytes.
- The gate needs `cmake`, `python3`, `gcc-13`/`g++-13` and read access to the
  pinned `upstream/llama.cpp` tree for the vendor inventory. No GPU, model,
  network or repository index.
- Native copies are inputs, never patched; the oracles re-read them on every run,
  so a change in `sync_vendor.py`, `license.cmake` or the CMake files surfaces as
  a gate failure instead of a silent divergence.

## Assumptions and recorded approximations

1. `lic_is_absolute(path, windows)` takes the host as an argument. The POSIX arm
   is gated against `cmake -P`; the Windows arm (drive letter, leading backslash)
   is the documented CMake rule and is listed as an approximation, because no
   Windows host is available to the gate.
2. `lic_resolve` passes the same platform flag; the native join itself is a
   literal `${CMAKE_CURRENT_SOURCE_DIR}/${FILE}`, so no trailing-slash logic is
   invented.
3. `vnd_pin_kind` recognises exactly four forms: `releases/latest/download`
   (floating), `refs/heads/` (branch), `refs/tags/` (tag) and a bare 40-hex path
   segment (commit). A bare branch name (`raw/master/`) and a PR ref
   (`refs/pull/123/head`) deliberately classify as "no pin": the real table never
   uses them, and treating them as pins is the failure mode being guarded.
4. `vnd_file_state` takes an `owned` flag from the caller: `CMakeLists.txt` and
   the two `httplib` files produced by `split.py` are written by the project, not
   downloaded, so they are admitted without a table entry.
5. `vnd_anchor_count` reproduces Python's non-overlapping `str.count()`; an empty
   anchor counts as 0 rather than `len+1`, and the exactly-once rule rejects it
   either way.
6. The gate counts anchors on the **checked-in** files, which are the patched
   ones. The upstream arm would need the reverse substitution; it is covered by
   the synthetic patch cases and by the `patched` arm over the real 265 KB
   `xxhash.h`.
7. `sys_ggml_plan` returns 2/1/0 for system/in-tree/parent-provided. The
   parent-provided outcome is the one boolean that `cmake -P` cannot produce
   (`TARGET` cannot be created in script mode) and is recorded as an
   approximation; the other two come from the native condition text.
8. The `lic_` body case runs `license_generate()` with `message` shadowed, because
   the native function writes `license.cpp` and only then raises `FATAL_ERROR` on
   the missing target. The generated bytes are the native bytes.
9. Platform split of the shipped tree: `work/llama.cpp-integration` vendors
   cpp-httplib **0.53.1** while the canon pins **0.54.1**. The gate runs against
   the canon; the divergence is recorded, not silently accepted, in `REPORT.md`.

## Conflicts and overlaps

- No overlap with `goose/misc/build-env.goose` (P10-05): that module owns the
  build directory, options, pins of the *upstream clones* and the composition
  list. This one owns third-party dependency admission, licences and vendor
  selection. The two share no helper name.
- `goose/json/json-parser.goose`, `json-dump.goose` and `goose/jinja/*` are cited
  as the owned replacements; no change is made to them here.
- `docs/goose-migration/tasks.json` P10-07 stays TODO until the lead accepts and
  integrates; its `slices_total`/`slices_done`/`remaining` mirror is the lead's
  step.

## SHA-256

Full lists: `results/hashes.txt` (artefacts), `results/native-sha256.txt`
(native inputs), `results/upstream-vendor-sha256.txt` and
`results/prod-vendor-sha256.txt` (both trees). Headline values:

| file | sha256 |
|---|---|
| `source/vendor-licence-payload.goose` | `3405622452bd2b658e6ac45ccf0bfcabd2b1ae3154f4f936e71d616d4fd87813` |
| `source/vendor-provenance.goose` | `9409fad9027aaa689a6a6cb1f7b4c4f485f2aef89a028e60506b51361125b65a` |
| `source/vendor-selection.goose` | `7a6cfeb577e2d7a043bc95765e9067e5f6b33ef42666bdeb858f2a29eb18dc5d` |
| `tests/cases/lic.tsv` | `e77ea7140a6840b6e800c7987d528105d0a68f0deefbce1975e68a57c6cbe825` |
| `tests/cases/vnd.tsv` | `861cf1eb2cdf0c72b6044358984e680634cb56e86dfd6e1ed08e265b503cf88a` |
| `tests/cases/sys.tsv` | `f9a610ba3fbca272acd9aa33d3b72833d43fbea65fbc711a94cf3f8444519721` |
| `tests/native/license.cmake` | `083515dff8a9b8696e348c7c616934e53941a7db30e1b3f092576632e9a2d1d4` |
| `tests/native/sync_vendor.py` | `b36dc1f59aff47892382028666444eab0b53309632275d0729867721dfd7b19e` |
| `tests/native/vendor-CMakeLists.txt` | `97242b05cf7d56231b05afa991036260025b9d6fe4f003dbc997dd7f5d2be06b` |

Toolchain: goose `1e714e6146bc` (`goose-2026.3-string`), `gcc-13`/`g++-13`
13.4.0, `cmake 4.2.3`, `Python 3.14.4`.
