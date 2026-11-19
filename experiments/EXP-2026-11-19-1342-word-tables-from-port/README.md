# EXP-2026-11-19-1342: the merge list and the token-type table also come from the port

## What was converted

Two more array reads in `llama-vocab.cpp`:

- the **merge list** (`tokenizer.ggml.merges`), read element by element through `fg_port_array_str`;
- the **token-type table** (`tokenizer.ggml.token_type`), materialised through `fg_port_array_int`.

With the vocabulary (EXP-2026-11-19-1341) all three arrays the vocabulary builder reads now come from the port.

## The rule that changed the implementation mid-way

The first version of the token-type conversion called an accessor that returned `0` on failure, and decided
"the port answered" by comparing against the native value - which cannot distinguish a legitimate zero from a missing
key. That is exactly the default-on-missing case the replacement plan forbids, and it was caught by reading the code
against the plan rather than by a test.

The accessor now reports success separately, and the whole table falls back to native unless **every** element is
answered:

```c
extern "C" bool fg_port_array_int(int64_t key_index, int64_t element, int64_t * out);
```

## Measured

```
decode  62.63 / 63.22 / 63.07
prefill 2691.5 / 2953.8 / 2939.2
hash    c2ff42cc1219
```

No type error, no warning about an invalid GGUF type for merges, and the response is byte-identical to the baseline.

## What is still native

`llama-impl`'s array walk (one site), `llama-adapter`'s two, the 6 tensor descriptor sites, the `const char *` key
form and the split-file path. Each is counted in `integration/p7-12-gguf-context/PLAN.md`.
