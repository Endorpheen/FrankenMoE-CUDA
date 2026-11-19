# EXP-2026-11-19-1429: shard naming verified, shard loading still untested

## What P7-12 asks and what the tree has

"Multi-file models, shard names, the union of offsets." The loader supports it (`llama_get_list_splits` at
`llama-model-loader.cpp:715`), and the naming lives in the library. What the tree does **not** have is a splitter:
`vendor/flint/tools/` holds only `mtmd`, `server` and `ui`, and `cmake --target help` has no split target.

## The naming, which can be tested without one

```
split_prefix("model-00002-of-00004.gguf", idx 1, n 4) -> "model"        (5 bytes)
split_path("model.gguf", idx 3, n 7)                  -> "model.gguf-00004-of-00007.gguf"
split_prefix("model.gguf", idx 0, n 2)                -> 0, refused
```

All three are the upstream convention: the prefix is the name up to the separator, the path is 0-based with the suffix
appended to the whole name, and a name that is not a split is refused rather than accepted.

## Two expectation errors in one probe

The first run failed on both counts: I expected `-00003-of-00007` for index 3, and I expected the prefix to include the
separator. The library was right both times. That is the third time in three cards - the flag probe, and now twice here -
and the rule stays the same: read what the code says, then compare.

## Why the loading itself stays untested

A hand-written splitter would make the test measure **the splitter**, not the loader: a wrong split produces a wrong file
and any conclusion drawn from it is about my tool. Building `gguf-split` needs an upstream checkout the tree does not
carry, and no small model with tensors is available to split.

So this card closes the naming half with evidence and records the loading half as blocked on a tool, not on the port.

## State

| item | state |
|---|---|
| shard naming (`split_prefix`, `split_path`) | verified, three cases |
| refusal of a non-split name | verified |
| loading a real multi-file model | **not measured** - no splitter, no split model |
