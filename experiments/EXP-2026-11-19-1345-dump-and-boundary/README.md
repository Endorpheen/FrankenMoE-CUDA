# EXP-2026-11-19-1345: the metadata dump reads arrays through the port; the adapter is a different file

## Converted: the metadata dump

`gguf_kv_to_str` in `llama-impl.cpp` renders a key for the model metadata listing, iterating string arrays element by
element. It now asks the port first, per element, and falls back to the native array:

```
decode  62.45 / 63.08 / 63.14
prefill 2696.9 / 2951.0 / 2951.4
hash    c2ff42cc1219
```

## Not converted, and not because it is hard: the adapter reads another file

`llama-adapter.cpp` walks a `UINT32` array - `alora_invocation_tokens` - but from a **LoRA adapter's** GGUF, not the
model's. The port's store holds the model file's window and records; it has never seen the adapter.

Serving that walk would need the port fed the adapter file as well, which is a different question from the model
loader port: a second document, a second window, and a decision about whether adapter metadata is in scope at all. It
is recorded here as out of scope for P7-12 rather than as unfinished work inside it.

## Remaining native surface, exactly

| item | status |
|---|---|
| split-file path | native, and **no port comparison exists**: the shard has its own GGUF context |
| LoRA adapter arrays | out of scope - different file, no window in the store |
| `GKV<const char *>` specialisation | never instantiated (EXP-2026-11-19-1344) |

For a single-file model - which is what every accepted measurement uses - the loader's metadata now comes from the
port, with the native context as a fallback that the gate exercises on every start.
