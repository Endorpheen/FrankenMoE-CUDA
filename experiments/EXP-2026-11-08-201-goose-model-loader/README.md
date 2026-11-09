# EXP201: model-loader shape decisions in Goose (APX: model loader)

First step into the loader, kept separate from the KV and graph work. Goose decides whether a
tensor found in the GGUF matches the shape the model asked for; the lookup, mmap and copy stay
native.

## Ported

| Goose | native | behaviour |
|---|---|---|
| `nelements` | `ggml_nelements` over a request | product of the first `n` dims |
| `dims_ok` | `llama-model-loader.cpp:907-915` | each requested dim must match exactly; unmentioned dims must be 1 |
| `shape_ok` | lines 896-916 | the two branches: element count when reshape is allowed, exact dims otherwise |
| `or_arr_too_many` | line 470 | `n > N_MAX` |
| `is_array_kv` | line 474 | `gguf_get_kv_type(...) == GGUF_TYPE_ARRAY` |
| `arr_length_ok` | line 479 | array length must equal the request |
| `use_mmap_mode` | line 558 | `MMAP \|\| MMAP_MLOCK \|\| AUTO` |
| `use_direct_io_mode` | line 559 | `== DIRECT_IO` |

`shape_ok` is routed into `llama-model-loader.cpp`.

## The `const` trap, hit again

`MAXD` was written as `const MAXD:i64=4`. The generated code reads consts from `gg_globals`, which
the emitter initialises inside `gg_program` -- and a bridge that calls a generated function
directly never runs that, so the const read as **zero**. Every call then trapped code 10
(`n <= MAXD` with `MAXD == 0`).

This is the identical defect EXP199 hit with `W`. Both are recorded in their STATUS/README files
because it is not a one-off: **any module whose bridge calls a generated function directly must
inline its constants as literals.**

## Evidence

    FG201L_MODEL_LOADER shape_checks=753 direct=0
    FG201L_KEYS load_mode=1 or_arr=6
    tokens and all 128 top-5 probabilities equal, 2538.40 / 25.68

753 tensors validated through Goose in one model load, `direct=0` -- the switch never fell back.
This is the same check whose earlier native path produced
`blk.1.attn_qkv.weight has wrong shape` when `is_recr` was wrong, so it is a live, load-bearing
decision rather than a formality.

Gate: 1205 checks / 0 failures.

## Switch

`GGML_GOOSE_MODEL_LOADER=0` restores the native shape checks.

## Next

`get_key` / `get_key_or_arr` type conversion and the hparams defaults; then the mmap span and
split-file selection.
