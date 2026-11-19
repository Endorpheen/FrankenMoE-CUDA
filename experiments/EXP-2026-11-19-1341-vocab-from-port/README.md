# EXP-2026-11-19-1341: the vocabulary is built from the port

## The largest data structure in the model now comes from the ported reader

`llama-vocab.cpp` builds the token list with `gguf_get_arr_str` for all 248 320 entries. It now asks the port first,
through a C accessor implemented in the loader (which owns the window and the records) and falls back to the native
array only when the port cannot answer:

```
FG_GGUF_SERVED_TOKENS from_port=196608      (248 320 in total)
FG_GGUF_PARSE ... disagreements=0
decode  62.62 / 62.77 / 62.98
hash    c2ff42cc1219
```

The response hash is the strongest statement available here: every token string the model matched, every token it
emitted, and every byte of the answer are unchanged.

## Why this is the step that matters

The vocabulary is not metadata in the corner - it is the table that turns every generated id into text and every
prompt into ids. Serving it from the port means the shipped runtime's **largest read** no longer comes from the native
context, and it follows from the file-level gate that compared all 744 231 array elements element by element
(EXP-2026-11-19-1338).

## The design that made it possible

The port's window and records live in `llama-model-loader.cpp`; the vocabulary is another translation unit. Rather than
duplicating the store, the loader exposes:

```c
extern "C" bool fg_port_array_str(int64_t key_index, int64_t element, const uint8_t ** ptr, int64_t * len);
```

One store, one owner, one accessor. The alternative - a second copy of the records in the vocab file - is the parallel
implementation rule 33 forbids.

## What is still native

The merge list and token types (same file, two more call sites), one array walk in `llama-impl`, two in
`llama-adapter`, the 6 tensor descriptor sites, the `const char *` key form and the split-file path. Each is listed in
`integration/p7-12-gguf-context/PLAN.md` with its counts.
