# EXP-2026-11-15-645 — P10-08 public headers, FFI and binary interoperability

Three model-free slices of the public ABI surface, ported to Goose and frozen by
differential gates that re-derive the native side from the base-commit sources on
every build.

- Requirement: **P10-08** "Preserve public headers, FFI and binary
  interoperability" (`docs/goose-migration/tasks.json`).
- Assigned base commit: `792293ad`. The working tree moved to `c344b309` while
  this experiment ran (other slices were integrated); nothing here depends on
  those commits. The header canon is `upstream/llama.cpp` at
  `0b5be7e4a25862bc2777d0c47eae18788a8c963a`, which is git-ignored and therefore
  fixed by that SHA and by the per-file SHA-256s in `results/hashes.txt`.
- Toolchain: `goose-2026.3-string` (git `e0501dc7`, runtime `1e714e61`),
  `gcc-13`/`g++-13` 13.4.0.
- No model, no GPU, no server, no production artifact was touched.

## Hypothesis

The part of `include/llama.h` and `ggml/include/*.h` that a non-C consumer
actually depends on is a set of tables and numeric constants, not behaviour:
block geometry per quantization type, the numeric value of every public enum and
limit, and the exact texts that exported `*_name` and `*_to_string` functions
return. Those can be ported and frozen without a model, and a component that
replaces the C++ implementation must reproduce them byte for byte or it silently
breaks every FFI caller and every GGUF file reader.

## Inventory and the three slices

Inventory covered: `include/llama.h` (1638 lines), `ggml/include/ggml.h` (2950),
`ggml/include/ggml-backend.h` (441), `ggml/include/gguf.h`, and the
implementation tables in `ggml/src/ggml.c`, `src/llama.cpp`,
`src/llama-model.cpp`, `src/llama-model-loader.cpp`.

Decisions that are checkable with no model and no weights were selected; the
rest (context, sampler chains, grammar) stays with its own requirements.

| slice | module | freezes | exports |
|---|---|---|---|
| S1 `abi_` | `source/ggml-type-table.goose` | `type_traits[GGML_TYPE_COUNT]`: per type name, block size, block bytes, quantized flag; `ggml_row_size` and 2-D `ggml_nbytes` arithmetic | 7 |
| S2 `hdr_` | `source/public-enum-values.goose` | 187 public constants: format magic/version macros, object geometry limits, `ggml_status`/`ggml_prec`/`ggml_op_hint`/`ggml_ftype`, every `llama_*` enum, and the two limits the API exposes as functions | 3 |
| S3 `nm_` | `source/public-name-tables.goose` | the text contract: `llama_ftype_name` (36 cases + fallback + the `(guessed) ` rule), `llama_flash_attn_type_name`, `llama_load_mode_name`, `llama_load_mode_from_str` (exact `strcmp` set), `llama_model_meta_key_str`, `ggml_status_to_string` | 6 |

Deliberate overlap: none. `enum ggml_type` is not repeated in S2 — its value is
the S1 table index and `abi_type_count()` is `GGML_TYPE_COUNT`, so the numbers are
frozen exactly once.

## The oracle

Nothing in a gate is compared against a hand-copied native table.

- `tests/extract_native.py` parses, on every build, `type_traits[...]` out of
  `ggml/src/ggml.c` (43 explicit slots, coverage of `0..42` asserted), the
  `ggml_status_to_string` cases, the four `llama_*` name switches and the two
  numeric limits, and emits C tables into `build/native/`. Block sizes and
  `sizeof(block_*)` stay as C expressions and are resolved by the compiler
  against the real `ggml.h` and `ggml-common.h`, so `QK_K` or a block struct
  change reaches the gate without any edit here.
- S2's native arm compiles `include/llama.h`, `ggml/include/ggml.h` and
  `ggml/include/ggml-backend.h` directly: `tests/native_keys.inc` is one
  `HDR(<identifier>)` line per frozen name and the compiler resolves each value.
- Compile-time freezes: `GGML_TYPE_COUNT == 43`, the twelve `QK*` block sizes and
  eight public structure sizes are `static_assert`ed in the fixtures (the
  structure sizes only on a 64-bit build).
- The port drives enumeration in S2: the gate asks the port for its key list and
  resolves every key natively, then walks the native map back and requires every
  native identifier in the port. Neither side can shrink silently.
- The port tables themselves were derived once from the same sources with
  `tests/gen_port_tables.py` (see `tests/templates/`) and are committed as
  literals; the mutation run is what proves the gate can see them change.

## Results

| slice | cases | values | failures at `-O1` | failures at `-O0` | mutations |
|---|---:|---:|---:|---:|---|
| `abi` | 106 | 274 | 0 | 0 | 4/4 |
| `hdr` | 197 | 384 | 0 | 0 | 4/4 |
| `nm` | 106 | 153 | 0 | 0 | 4/4 |
| **total** | **409** | **811** | **0** | **0** | **12/12** |

Mutations: halves 6/6 and 6/6, module generated and compiled at `-O0`; logs in
`results/mutate-1.log` and `results/mutate-2.log`. Byte-identical summaries at
both optimisation levels (`results/gate-o1.log`, `results/gate-o0.log`,
`results/gate-final.log`).

## Reproduce

```
bash tests/build.sh                    # all three gates at -O1
bash tests/build.sh hdr                # one slice
bash tests/build.sh nm mutate          # one slice, module at -O0
python3 tests/mutate.py 1              # mutations, first half
python3 tests/gen_port_tables.py --inject   # regenerate port tables from canon
```

Overrides: `FG_NATIVE` (llama.cpp checkout), `FG_GOOSE`, `CC`, `CXX`, `FG_OUT`.
No absolute scratch path is used; the experiment root comes from the script
location. Build products land in `build/` and are not committed.

## Exact integration step

1. Copy this directory to `experiments/EXP-2026-11-15-645-p10-08-public-headers-abi/`.
2. Add three rows to `CANONICAL_SOURCES` in `tools/sync-goose-tree.py`:
   - `goose/model/ggml-type-table.goose`, this experiment, `source/ggml-type-table.goose`, native description `ggml/src/ggml.c (type_traits, blck/type size, row size)`, gate `106 cases / 274 values / 0 failures, 12/12 mutations (shared)`;
   - `goose/misc/public-enum-values.goose`, `source/public-enum-values.goose`, native `include/llama.h + ggml/include/ggml.h + ggml/include/ggml-backend.h (public constants)`, gate `197 cases / 384 values / 0 failures`;
   - `goose/misc/public-name-tables.goose`, `source/public-name-tables.goose`, native `src/llama.cpp, src/llama-model.cpp, src/llama-model-loader.cpp, ggml/src/ggml.c (name and status texts)`, gate `106 cases / 153 values / 0 failures`.
3. `python3 tools/sync-goose-tree.py && python3 tools/check-goose-tree.py`.
4. Add the three modules to `MODULES` in `tools/build-main-runtime.sh` (after the
   `goose/model/quant-*` entries) and one set to `tools/check-composition.sh`:
   `check abi "$ROOT"/goose/model/ggml-type-table.goose "$ROOT"/goose/misc/public-enum-values.goose "$ROOT"/goose/misc/public-name-tables.goose`.
5. Ledger: P10-08 becomes `PARTIAL` with `slices_total: 3`, `slices_done: 3`,
   `coverage_doc: docs/goose-migration/P10-08-COVERAGE.md`, and `remaining`
   listing the parts below that these three slices do not close.

## Compatibility

- `goose check` passes for each module and for the composition of all three.
- Composition with the 66-module main set (`tools/compose-goose.py` + `goose
  check`): typechecked, 3059 specializations. The composed unit was also generated
  to C and compiled with `gcc-13 -O1` (3.9 MB text section) with no duplicate
  global definition, and all 16 exports are present.
- No duplicate names: zero collisions of any function name (exported or not)
  against the 7902 function names in the canonical `goose/` tree, so the tool's
  per-module rename table does not need an entry.
- Rebase note: the gates depend only on `upstream/llama.cpp` and the Goose
  toolchain, so a rebase of the repository onto a newer canon does not change the
  numbers. Re-run `bash tests/build.sh all` after any canon move anyway.

## Assumptions and boundaries

- Native accessors assert on an out-of-range type and on `blck_size == 0`
  (`ggml_row_size(4, …)` divides by zero). The port returns `-1` for those inputs;
  the gate checks the port's refusal separately and never compares it to a native
  call that would trap.
- `llama_load_mode_from_str` throws `std::invalid_argument`; an exception cannot
  cross a C ABI, so an unknown string returns `-99`. The gate compares the
  accepted set exactly and checks the refusal as a refusal.
- `llama_flash_attn_type_name` and `llama_load_mode_name` call `GGML_ABORT` on an
  unknown value, `llama_model_meta_key_str` returns `nullptr`. Those branches are
  not executed natively; the gate checks that the port refuses.
- `abi_row_size` computes `type_size * ne / blck` in the native order. The corpus
  keeps the product below 2^63, which is far above any real tensor row.
- S2's structure sizes are x86-64 LP64 facts and are asserted only when
  `sizeof(void*) == 8`.
- Not covered by these slices, and therefore still open for P10-08: the rest of
  the header surface (`ggml_backend_*` function-pointer tables, `ggml-opt.h`,
  `gguf.h` writer/reader declarations), `llama_progress_callback` semantics,
  symbol visibility/versioning of the shared objects, and the ABI of the
  `carrier/runtime/core/include` headers.

## Files

```
source/ggml-type-table.goose        S1, abi_
source/public-enum-values.goose     S2, hdr_
source/public-name-tables.goose     S3, nm_
tests/extract_native.py             native oracle from the base-commit sources
tests/gen_port_tables.py            one-shot derivation of the committed tables
tests/templates/*.tmpl              the modules with @@TABLE@@ markers
tests/native_keys.inc               185 header identifiers for the S2 native arm
tests/fixture_abi.cpp               S1 gate
tests/fixture_hdr.cpp               S2 gate
tests/fixture_nm.cpp                S3 gate
tests/build.sh                      gate build, -O1 final / -O0 mutate
tests/mutate.py                     12 mutations, halves 6/6
results/                            gate and mutation logs, hashes
```

SHA-256 of the artifacts this experiment adds:

```
c2bbe6e9f4c415e8b37460214131553cc2cfa155ac8d8cbdabd3af8806a53dd6  source/ggml-type-table.goose
e8e5d05a2a34be46c478dfc369bbb4b5dadd625ae56284c81baeb101d9731973  source/public-enum-values.goose
12514b4b363d80ef992d485ff94a79c9a76269346b31bbb0add579b225e6d336  source/public-name-tables.goose
d4f182f1d07fdc2563b3164e60fe6638a335cb412248ff2d6fe2a7cc8d20879a  tests/extract_native.py
6375d7e136a3c73def5cd3db558ee199e06150ef550b30d37850b4555c5d3ffe  tests/gen_port_tables.py
b853476deeda514db9a34753e29da171c35dc6b8ff5593457d9d97b467edf335  tests/mutate.py
8330e8d643d3e52c2ac453db38c12732a55e429f59d662627e5e5382965b214a  tests/fixture_abi.cpp
26851fe8af55fb5da3343ed6227274164859f3c9066c262f718ce949e09efba7  tests/fixture_hdr.cpp
6568075969cd6c20aaa601b4841899018264f96f45889396e02035a5f1521382  tests/fixture_nm.cpp
ccdcb2e95fee0766e458db06d9957f1980cc69774e3b404314ba570ae72fe894  tests/build.sh
```

SHA-256 of every artifact and of the native inputs is in
`results/hashes.txt`.
