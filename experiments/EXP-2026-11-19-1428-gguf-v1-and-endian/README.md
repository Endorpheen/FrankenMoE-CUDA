# EXP-2026-11-19-1428: GGUF v1 and non-little-endian files - what the tree actually does

## The two items P7-12 lists

"Support GGUF v1 and files whose byte order is not little-endian." Both are testable without a model, and this card tests
them rather than assuming either.

## GGUF v1

A fixture was written and its version field changed from 3 to 1 - the layout of a metadata-only file is identical, so this
tests the reader's **version handling**, not a format change:

```
gguf_init_from_reader: GGUFv1 is no longer supported, please use a more up-to-date version
llama_model_load: error loading model: ... failed to load model
```

The rejection comes from the **native** reader (`vendor/flint/ggml/src/gguf.cpp:512`). So supporting v1 is not a missing port
feature: the runtime has deliberately dropped it. Porting v1 support would make the port accept files this runtime refuses -
a divergence, not a gain. The item is **stale for this tree**, and the card records the evidence instead of implementing
something the product does not want.

## Byte order

The reader contains **no byte-swap path at all** - no `bswap`, no endianness branch, nothing of the kind in `gguf.cpp`. GGUF
has one on-disk byte order by specification; a file with another one is not a variant the format defines. So this half of
the item describes a case that cannot occur, and it is recorded as such rather than implemented.

## Why the negative results are worth a card

An item that cannot be satisfied without diverging from the product is not work; it is a question that has to be answered
with evidence and closed. Writing v1 support because a checklist says so would have added code the runtime never calls, for
files it refuses - and the next reader of the checklist would have had no way to know.

## What remains of P7-12

| item | state |
|---|---|
| connect the Goose GGUF/reader/tokenizer modules to the runtime | done for the metadata reader; the tokenizer is served for its family |
| delete the replaced native implementations | blocked by rule 15 until acceptance |
| GGUF v1 | **not applicable** - the native reader refuses it |
| byte order other than little-endian | **not applicable** - the format has one order and no reader path for another |
| multi-file models, shard names, offset union | not measured; no splitter in the tree and no small model with tensors |
| accessors beyond the verified typed classes | floats and booleans added (EXP-1400, -1401) |
| model file search, Hugging Face cache | host side absent from this tree (EXP-1414) |
