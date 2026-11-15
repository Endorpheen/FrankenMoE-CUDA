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

## Remaining

`ggml_backend_*` and `gguf.h` headers, backend headers, `.so` SONAME/export
symbol compatibility against `abi-ledger.json`, and callback semantics.

## Boundaries

`quant-types.goose` keeps its own `T_*` constants with the same numbers; there is
no collision, but merging them into the ABI table is a decision for the owner.
