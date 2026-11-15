# EXP-2026-11-15-651 — P10-08 backend/GGUF headers, SONAME and export surface

Three model-free slices that continue `EXP-2026-11-15-646`: the backend
public header, the GGUF public header, and the shared-object identity
(SONAME, versioning, export surface) of the built libraries.

- Requirement: **P10-08** "Preserve public headers, FFI and binary
  interoperability" (`docs/goose-migration/tasks.json`), currently `PARTIAL`.
- Assigned base commit: `1b3f4a1a`. Header canon: `upstream/llama.cpp` at
  `0b5be7e4a` (git-ignored, anchored by the SHA-256s in `results/hashes.txt`).
- Reference shared objects: `build/exp054-dflash/bin/*.so.*` (untracked local
  build; SHA-256s in `results/hashes.txt`, `FG_SO_DIR` overrides the path).
- Toolchain: `goose-2026.3-string`, `gcc-13`/`g++-13` 13.4.0.
- No model, no GPU, no server, no production artifact was touched.

## Hypothesis

What EXP-646 left open is not more tables of numbers: it is (a) the second and
third public header family (`ggml-backend.h`, `gguf.h`) whose contracts are
enums, struct layouts, callback shapes and a byte format, and (b) the identity
of the shared objects themselves, where the SONAME, the symlink chain, the
`DT_NEEDED` set and the exported symbol set decide whether a consumer that was
built once can still bind. Both are decisions that can be frozen without a model
and checked against the real headers and the real `.so` files.

## Slices

| slice | module | freezes | exports |
|---|---|---|---|
| S4 `bkh_` | `source/backend-headers.goose` | 39 backend-header values (buffer usage, five device types, meta device ceiling 16, eight split axes incl. 10/11/98/99, sizes and every field offset of the five public structs) plus the shape (arity, return class, parameter classes) of the eleven public callback typedefs and the eight split-axis name texts | 8 |
| S5 `ggh_` | `source/gguf-headers.goose` | 25 GGUF-header values (four magic bytes, the 32-bit magic word, version 3, alignment 32, the 17-byte alignment key, thirteen value types and `GGUF_TYPE_COUNT`, `gguf_init_params` layout), the thirteen type names and element sizes, the alignment rule (non-zero power of two), the data-offset rule (padded iff the file has tensors) and the reader-callback chunking rule | 13 |
| S6 `son_` | `source/soname-exports.goose` | the version variables of the three CMakeLists, `lib<stem>.so.<soversion>` and the `.so -> .so.<sov> -> .so.<version>` chain for seven libraries, required and forbidden `DT_NEEDED` entries, the header-to-library export mapping, the enumerated newer-than-build allowance, the six undeclared `llama_*` exports, the ledger counts, the three export-macro expansions and a SONAME parser | 26 |

## The oracle

Nothing is compared against a hand copy where a machine can resolve it:

- `tests/extract_native.py` parses, on every build:
  - the split-axis switch out of `ggml-backend-meta.cpp`, resolving the
    enumerators from `ggml-backend.h`;
  - `GGUF_TYPE_NAME` and `GGUF_TYPE_SIZE` out of `gguf.cpp` (asserting thirteen
    types and `GGUF_TYPE_COUNT == 13`);
  - `GGML_VERSION_*` and `LLAMA_VERSION_*` out of the two `CMakeLists.txt`;
  - `SONAME`, `DT_NEEDED` and the dynamic symbol table of the seven reference
    `.so` files with `readelf` and `nm`, and the two symlink targets per library
    with `readlink`;
  - the 462 public declarations out of the five headers;
  - the six export-macro expansions by running the real preprocessor over the
    real headers with and without `-DGGML_SHARED`/`-DGGML_BACKEND_SHARED`/
    `-DLLAMA_SHARED`;
  - the ledger counts from `docs/goose-migration/abi-ledger.json`.
- The native arm of each fixture compiles the real headers: `sizeof`, every
  `offsetof`, the enum values, the struct sizes as `static_assert`, and the
  callback shapes by template deduction over the real typedefs.
- The GGUF and backend runtime arms `dlopen` the reference `libggml-base.so` and
  drive it: `gguf_type_name`, `gguf_init_empty`, `gguf_get_meta_size`,
  `gguf_get_meta_data`, `gguf_get_version`, `gguf_get_alignment`,
  `gguf_get_data_offset`, `gguf_init_from_callback` over crafted files (with and
  without the alignment key, with and without a tensor), and
  `ggml_backend_meta_split_axis_name`. Invalid alignments, a short read and a
  null callback are exercised natively and must be refused.

## Results

| slice | cases | values | failures at `-O1` | failures at `-O0` | mutations |
|---|---:|---:|---:|---:|---|
| `bkh` | 65 | 184 | 0 | 0 | 12/12 |
| `ggh` | 65 | 239 | 0 | 0 | 12/12 |
| `son` | 498 | 570 | 0 | 0 | 12/12 |
| **total** | **628** | **993** | **0** | **0** | **36/36** |

Mutation halves are 6/6 per slice (18 mutations per half), module generated and
compiled at `-O0`; logs in `results/mutate-1.log` and `results/mutate-2.log`.
The three summaries are byte identical at both optimisation levels
(`results/gate-o1.log`, `results/gate-o0.log`, `results/gate-final.log`).

## Facts worth naming

- **The llama surface is complete, with six extras.** All 235 `LLAMA_API`
  declarations in `llama.h` are exported by `libllama.so.0.3.0`. The library also
  exports six `llama_*` symbols that no header declares:
  `llama_copy_state_data`, `llama_get_expert_cache_stats`,
  `llama_get_state_size`, `llama_load_session_file`, `llama_save_session_file`,
  `llama_set_state_data`. A consumer can bind to them, so they are enumerated
  instead of ignored.
- **One backend declaration is newer than the reference build.**
  `ggml_backend_op_alloc_size_may_expand` is declared at
  `ggml-backend.h:429` and defined at `ggml-backend.cpp:2114`, but the reference
  `.so` predates it. The gate treats it as an enumerated allowance: any other
  unresolved declaration fails, and the allowance cannot grow silently.
- **The backend header spans two libraries.** 99 of 116 declarations live in
  `libggml-base.so`, 16 registry entry points in `libggml.so`; neither is
  complete alone.
- **SONAME identity.** `ggml*` is version `0.22.0` with SOVERSION `0`; `llama`,
  `llama-common` and `mtmd` are `0.3.0` with SOVERSION `0`. Every library is
  `lib<stem>.so.0`, and `libllama.so.0` requires `libggml.so.0` and
  `libggml-base.so.0` but never a device backend.
- **The reconstructed-runtime ledger names a different library.**
  `abi-ledger.json` describes `libllama-exp371.so.0.7.0` with 307 exports and 165
  declarations; the current surface is `libllama.so.0` with 241 `llama_*`
  exports and 235 declarations. The port's SONAME parser and identity check
  record the difference rather than pretending the two are interchangeable.
- **Export macros differ in more than visibility.** `GGML_API` and
  `GGML_BACKEND_API` expand to `__attribute__((visibility("default"))) extern`,
  `LLAMA_API` to `__attribute__((visibility("default")))` with no `extern`. Both
  the visibility and that difference are frozen (six macro/configuration cells).
- **GGUF geometry.** The magic is `47 47 55 46` (`GGUF`, 0x46554747 little
  endian), the version is 3, the default alignment is 32, and the alignment key
  is 17 bytes, not 18. The writer pads the metadata of an empty context to 32
  bytes, and the reader pads the data section only when the file has tensors.
- **Reader-callback protocol.** Each parser request is split into chunks of
  `min(max_chunk_read, remaining)`; a full chunk continues, a short chunk is
  end-of-file and stops the request without retry, and `max_chunk_read == 0`
  means no limit. The parse itself issues one request for the 4-byte magic, then
  4, 8 and 8 bytes for the header fields, so the observed call sequence for a
  24-byte file at chunk 7 is `[1, 1, 1, 1, 4, 7, 1, 7, 1]`, not `ceil(24/7) = 4`.
  That distinction is what the runtime arm checks.

## Reproduce

```
bash tests/build.sh                    # all three gates at -O1
bash tests/build.sh ggh                # one slice
bash tests/build.sh son mutate         # one slice, module at -O0
python3 tests/mutate.py 1              # half 1: 6+6+6
python3 tests/mutate.py 2              # half 2: 6+6+6
```

Overrides: `FG_NATIVE`, `FG_GOOSE`, `CC`, `CXX`, `FG_OUT`, `FG_SO_DIR`,
`FG_LEDGER`, `FG_REPO`. No absolute scratch path is used; the experiment root
comes from the script location. Build products land in `build/` and are not
committed.

Rebuilding the reference objects (needed only on a fresh machine):

```
cmake -S upstream/llama.cpp -B build/exp054-dflash -DGGML_CUDA=ON -DLLAMA_CURL=OFF
cmake --build build/exp054-dflash -j
```

## Exact integration step

1. Copy this directory to
   `experiments/EXP-2026-11-15-651-p10-08-backend-gguf-soname/`.
2. Add three rows to `CANONICAL_SOURCES` in `tools/sync-goose-tree.py`
   (after the `misc/public-name-tables.goose` row):
   - `("misc/backend-headers.goose", "EXP-2026-11-15-651-p10-08-backend-gguf-soname", "source/backend-headers.goose", ...)`;
   - `("misc/gguf-headers.goose", ... "source/gguf-headers.goose", ...)`;
   - `("misc/soname-exports.goose", ... "source/soname-exports.goose", ...)`.
3. `python3 tools/sync-goose-tree.py && python3 tools/check-goose-tree.py`.
4. Add the three modules to `MODULES` in `tools/build-main-runtime.sh` (after
   the `goose/misc/public-*` entries) and one set to
   `tools/check-composition.sh`:
   `check abi2 "$ROOT"/goose/misc/{backend-headers,gguf-headers,soname-exports,public-enum-values,public-name-tables,ggml-type-table}.goose`.
5. Ledger (`docs/goose-migration/tasks.json`, P10-08): `slices_total: 6`,
   `slices_done: 6`, `remaining: ["carrier/runtime/core/include headers",
   "host wiring of the ABI and callback slices (P7-10/P9-03)"]`,
   `coverage_doc` unchanged; refresh `status_totals`, `updated_at` and
   `work_front`. Update `docs/goose-migration/P10-08-COVERAGE.md` with the three
   new rows and the new gate numbers (628 cases / 993 values).

## Compatibility

- `goose check` passes for each module (69, 74 and 91 specialisations).
- Zero name collisions: the 55 functions of the three modules against the 8233
  function names in the canonical `goose/` tree, exported or not, so
  `tools/compose-goose.py` needs no rename entry.
- The exports are `bkh_*`, `ggh_*`, `son_*`; none appears in
  `tools/goose-symbols.txt`.
- Rebase note: the gates depend on `upstream/llama.cpp`, the local reference
  build and the Goose toolchain, not on the repository working tree, so a rebase
  does not change the numbers. Re-run `bash tests/build.sh all` after a canon
  move; a moved canon will legitimately change the version and export counts.

## Assumptions and boundaries

1. The canon and the reference `.so` set are git-ignored local artifacts,
   anchored by SHA-256. The gates fail loudly with a clear message when either
   is missing; they never fall back to a hand copy.
2. The reference build is older than the header canon; the one resulting gap is
   the enumerated allowance. Any second gap fails.
3. The Windows branches of the export macros are not exercised: the preprocessor
   arm covers the Linux expansion only. Codes 2/3 of the visibility model are
   the `LLAMA_API` pair, not a platform pair.
4. `ggml_backend_meta_split_axis_name` aborts natively on an unknown axis and
   `gguf_type_name` returns `nullptr`; the port returns `-1` for both, and the
   gate checks refusal as refusal, never by calling the aborting branch.
5. Struct sizes are x86-64 LP64 facts; the callbacks are classified by kind
   (void/bool/int/size_t/fp/pointer/struct), so a parameter renamed from `int`
   to `int64_t` is not a break here, while arity, pointer-ness and struct
   return are.
6. The ledger counts are frozen as ledger facts of the reconstructed runtime,
   not as properties of the upstream libraries.
7. Not covered, and therefore still open for P10-08: the per-backend headers
   (CUDA/Metal/Vulkan/...), `ggml-cpp.h`, the rest of `ggml-opt.h`'s behaviour,
   `carrier/runtime/core/include`, and the full scheduler eval-callback
   sequencing at run time (only its typedef shape is frozen; the protocol needs
   a real backend).

## Files

```
source/backend-headers.goose   S4, bkh_
source/gguf-headers.goose      S5, ggh_
source/soname-exports.goose    S6, son_
tests/extract_native.py        oracle: sources, CMake, readelf, nm, cpp, ledger
tests/fixture_bkh.cpp          S4 gate
tests/fixture_ggh.cpp          S5 gate (drives the reference .so)
tests/fixture_son.cpp          S6 gate
tests/native_bkh.inc           39 identifiers resolved by the compiler
tests/native_ggh.inc           20 identifiers resolved by the compiler
tests/build.sh                 gate build, -O1 final / -O0 mutate
tests/mutate.py                36 mutations, halves 6/6 per slice
results/                       gate and mutation logs, hashes
```
