# EXP-2026-11-19-1343: the tensor lookup and its type come from the port

## What was converted

The metadata-only path of `get_tensor_meta` looks a tensor up by name and reads its type:

```cpp
int64_t tid = fg_port_find_tensor(name, len);
int64_t fg_type = 0;
if (tid >= 0 && fg_port_tensor_type(tid, &fg_type)) { type = (ggml_type) fg_type; }
else { /* native lookup and gguf_get_tensor_type */ }
```

Both directions are guarded: a name the port cannot resolve falls back to the native context, and a descriptor it
cannot describe falls back as well.

## A correction to the plan's own numbers

The plan said **6** places use the tensor descriptor API. Grepping two patterns (`gguf_find_tensor` and
`gguf_get_tensor_*`) matched the same site twice, and the real count is **1**. The plan now carries the corrected
number, because an inflated work estimate is its own kind of wrong record - and it would have made a one-site
conversion look like a project.

## Measured

```
decode  62.43 / 63.23 / 63.02
prefill 2683.6 / 2949.2 / 2943.6
hash    c2ff42cc1219
```

## What is still native

`const char *` keys, two array walks outside the vocabulary (`llama-impl`, `llama-adapter`), and the split-file path.
Those are the whole remainder of the loader port.
