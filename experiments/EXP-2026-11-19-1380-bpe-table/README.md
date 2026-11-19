# EXP-2026-11-19-1380: the encoder's table, prepared and inert, and the cost that blocks it

## What is in the tree

`vendor/flint/src/llama-model-loader.cpp` now carries, behind `FG_TK_BPE_DIAG`:

- `fg_bpe` - the token and merge arrays plus the slot tables, sized from the native reader's own counts;
- `fg_bpe_init(key_tok, key_mrg, n_tok, n_mrg)` - fills the offsets from the port's reader and calls `bt_build`;
- `fg_bpe_encode(text, n, ids, cap)` - calls `bt_bpe`, ready to be the shadow comparison.

With the flag unset nothing runs and nothing is allocated: the shipped path is unchanged.

## Why it is not finished

The offsets are taken one element at a time through `fg_port_array_str`. With 248 320 tokens and 247 587 merges that is
half a million calls, and a metadata-only load did not finish inside 200 seconds:

```
FG_TK_BPE_ENTRY build=1 port=1 n_tok=248320 n_mrg=247587
FG_TK_BPE_OFF tok i=0 failed      <- the earlier reader-index mismatch, fixed
```

so the array lookup itself is the remaining suspect: `gr_arr_str_span` is called per element and its own cost per call
is what has to be measured before another attempt. That is a five-minute measurement on a small range, not a
twenty-minute model load.

## The faster check this produced

`llama_model_params.vocab_only = true` loads the metadata and the vocabulary without the weights. Every encoder check
was paying three to four minutes for 20 GB it never used; the harness at `load_only.c` is the same check in seconds. It
is now the default for this kind of verification, and the reason the earlier loop was slow.

## State

| piece | state |
|---|---|
| table sizing, from the reader's counts | done |
| offsets | **not usable yet** - per-element cost unmeasured |
| `bt_build` call | compiles and resolves, not reached |
| shadow comparison at `tokenize` | not written |
| shipped path | untouched, inert behind the flag |
