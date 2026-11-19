# What still reads GGUF natively, and what of it is verified

An inventory with counts taken from the tree, not from memory. It answers one question per row: if this read were
wrong, would anything notice?

## In the model loader (`vendor/flint/src/llama-model-loader.cpp`)

| read | count | verified at load | how |
|---|---|---|---|
| `gguf_find_key` | 11 | yes | every key's index compared with the port's (`FG_GGUF_LOOKUP`) |
| `gguf_get_kv_type` / `gguf_type` | 17 | yes | type per key compared (`FG_GGUF_PARSE`) |
| `gguf_get_val_u*` / `gguf_get_val_i*` | 17 | yes | value compared in `FG_GGUF_VALUES`, 34/34 on Apex |
| `gguf_get_val_f32` / `f64` | 4 | yes | compared **bit for bit** through `gr_meta_f32_bits` (EXP-1400) |
| `gguf_get_val_bool` | - | yes | compared as 0/1 (EXP-1400) |
| `gguf_get_val_str` | 4 | yes | 13/13 string keys compared, byte for byte |
| `gguf_get_arr_n` | 8 | yes | length implied by the element-wise comparison |
| `gguf_get_arr_data` / `arr_str` | 8 | yes | 744 231 array elements compared element by element |
| `gguf_get_key` / `gguf_type_name` | 7 | yes | names compared through the lookup check |

## In the vocabulary (`vendor/flint/src/llama-vocab.cpp`)

| read | count | verified | how |
|---|---|---|---|
| `gguf_find_key`, `gguf_get_kv_type`, `gguf_get_arr_type`, `gguf_get_arr_n` | 28 | yes | the loader's key/type comparison covers the same keys |
| `gguf_get_arr_data` for scores and types | 5 | yes | same arrays are compared element-wise by the loader |
| `gguf_get_arr_str` for tokens and merges | 3 | yes | 248 320 + 247 587 elements compared |

The vocabulary still **reads** through the native API; it is not yet served by the port. That is a migration item, not a
correctness hole: the values it reads are the values the loader has already proven equal.

## The one class not compared

**Float arrays.** The element-wise section compares string arrays and integer arrays
(`et >= GGUF_TYPE_UINT8 && et <= GGUF_TYPE_INT32`); an array of `FLOAT32`/`FLOAT64` falls through. Apex's metadata has
its floats as scalars, which are compared, so nothing is unchecked on this model - but another model with a float array
would be, and the gap is recorded rather than assumed empty.

## What this inventory implies

1. no read in the loader is unverified on Apex except float arrays, which Apex does not have;
2. the vocabulary's reads are verified indirectly, because they touch the same arrays;
3. serving the vocabulary's reads from the port is the remaining migration work, and it needs the array doors the
   encoder already uses.
