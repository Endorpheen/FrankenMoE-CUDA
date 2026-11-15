# P10-08 — public headers, FFI and binary interoperability

Status: **PARTIAL** — 3 of 3 slices accepted; the headers, SONAME/export
compatibility and callback semantics remain open.

## Slices

| slice | content | exports |
|---|---|---|
| S1 ggml type table | `type_traits[43]`: name, blck_size, type_size, is_quantized, row_size, nbytes | 7 `abi_*` |
| S2 public constants | 187 values: format magics/versions, limits, `ggml_status`/`prec`/`op_hint`/`ftype`, all `llama_*` enums, `llama_max_devices=16`, `max_tensor_buft_overrides=4096` | 3 `hdr_*` |
| S3 public name tables | `llama_ftype_name` (with the `(guessed) ` prefix), `load_mode_name`/`load_mode_from_str` (exactly six strings, strcmp), flash-attention type name, meta keys, `ggml_status_to_string` | 6 `nm_*` |

## Evidence

- Experiment: `experiments/EXP-2026-11-15-646-p10-08-public-headers-abi`.
- Canon: `goose/misc/{ggml-type-table,public-enum-values,public-name-tables}.goose`.
- Gates: 106/274 + 197/384 + 106/153 values, 0 failures, byte-identical at -O0.
- Mutations: 12/12 (halves 6/6).
- The oracles extract the tables from the native sources at build time
  (`ggml.c`, `llama.cpp`, `llama-model.cpp`, `llama-model-loader.cpp`) and freeze
  public struct sizes with compile-time asserts.

## Additional slices (EXP649)

| slice | content | exports |
|---|---|---|
| S4 backend header | 39 constants/layout records, struct offsets 5/56/16/2128/32, 11 callback typedef shapes, 8 axis names | 11 `bkh_*` |
| S5 GGUF header | magic `0x46554747`, version 3, alignment 32, 17-byte key, 13 types, power-of-two and pad rules, reader callback protocol | 7 `ggh_*` |
| S6 SONAME and exports | 7 library SONAME/symlink chains, required/forbidden `DT_NEEDED`, 462 declarations vs `nm`, 6 declared-less `llama_*` exports | `son_*` |

Evidence: 628 cases / 993 values, 0 failures, byte-identical at -O0; mutations
36/36. Findings: all 235 `llama.h` declarations are exported, but 6 symbols
(`llama_save_session_file` and siblings) are exported without declarations;
`ggml_backend_op_alloc_size_may_expand` is newer than the reference build and is
an explicit exception; the backend header splits 99/16 between `libggml-base.so`
and `libggml.so`; the ledger describes `libllama-exp371.so.0.7.0` against
`libllama.so.0`; the GGUF key is 17 bytes, not 18.

## Additional slices (EXP654)

| slice | content | exports |
|---|---|---|
| S7 device headers | 14 headers, 14 macros, all 84 `GGML_BACKEND_API` declarations with arity/return/parameter classes, 12-capability matrix | 69 `devh_*` |
| S8 `ggml-cpp.h` | 7 RAII wrappers with release-call counts, includes, 3 carrier enums, 55 config defaults, layout facts | `cpph_*` |
| S9 scheduler-eval | ask/observe/cancel machine, event order, cancel, two eval callback shapes, 6 log levels with routing, progress contract | `sched_*` |

Evidence: 143/620 + 78/435 + 21/9482 values, 0 failures, byte-identical at -O0;
mutations 36/36. The oracle re-reads the headers and the `ggml-backend.cpp` loop
each build, expands macros with the compiler, uses `decltype` and `--wrap`, and
measures log routing through `libggml-base`. Two facts: `ggml_log_set(nullptr)`
reinstalls the default callback instead of clearing the slot, and
`ggml-hexagon.h` declares no buffer type while `ggml-virtgpu.h` declares only
`reg`.

## Remaining

Host wiring of the ABI/header slices; the decision on the SONAME ledger library
mismatch.

## Boundaries

`quant-types.goose` keeps its own `T_*` constants with the same numbers; there is
no collision, but merging them into the ABI table is a decision for the owner.
