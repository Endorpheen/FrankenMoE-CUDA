# Replacing the loader's GGUF context with the ported reader

Status: prepared, not started. Every measurement it depends on exists (below); the edit itself is large enough
that it must be done as a block with its own acceptance, not appended to a session.

## What already exists

| piece | evidence |
|---|---|
| the ported reader, proven against the native one on the real 20 GB file | header, 52 typed keys, 744 231 array elements, 753 tensor names, 753 descriptors, 753 tensor sizes - all equal (EXP-2026-11-19-1327) |
| the reader composed into the shipped unit | 15 `gr_*` doors, including `gr_tensor_info` (EXP-2026-11-19-1337) |
| a call path from the loader | `fg_gguf_check` in `llama_model_loader`, behind `FG_GGUF_DIAG` (EXP-2026-11-19-1335) |
| a load-time gate | 1610 comparisons per start, refusing the load on any disagreement |

## What has to change

`llama_model_loader` holds `struct gguf_context * metadata` and reads everything from it:

- **17 call sites** of `get_key(...)`, which today resolve a key name to a value through the native context;
- **6** places that use `gguf_find_tensor` / `gguf_get_tensor_*` for descriptor fields;
- the split-file join, which builds additional contexts per shard;
- `n_kv` / `n_tensors` counters.

The replacement is therefore a **loader port**, not a substitution: the port's records become the store, and each of
those 23 sites reads from the doors instead of the context. That is the unit the rule-33 goal needs - one source of
truth, no parallel implementation.

## Order of work

1. keep the window and the port records alive in the loader after `fg_gguf_check` (state, kv, tensors, slots);
2. add a private `fg_key_int(key)` / `fg_key_string(key)` pair backed by `gr_find_kv` + `gr_meta_int` / `gr_val_span`,
   and a `fg_tensor_desc(idx)` backed by `gr_tensor_info`;
3. convert the 17 `get_key` sites **by kind**: integers first (26 keys are proven equal), then strings, then arrays;
4. convert the 6 descriptor sites;
5. split files last: a shard is a separate context and needs the same treatment, or the split path stays native and is
   recorded as such;
6. remove `gguf_init_from_file` from this path only when the gate has nothing left to compare.

## The gate that guards each step

`FG_GGUF_DIAG=1` already refuses the load on any disagreement, so every conversion step is checked by the load itself:
a site that reads the wrong field fails immediately rather than at generation. After each step:

```
FG_GGUF_DIAG=1 <server> ...        # must load, 1610 comparisons, 0 disagreements
apex-test.sh <tag> 512 4096 4096 4992    # hash c2ff42cc1219, decode within the 60.4-63.5 spread
```

## Risks, in the order they are likely

1. **strings and arrays**: only 13 string keys and 5 arrays are proven element-wise; the loader reads a string
   (`general.architecture`) and arrays (tokenizer tables), so those conversions need their own comparison at the
   moment of use, not only at file level;
2. **split files**: the shard path has no port comparison at all today; converting it is a separate unit;
3. **silent fallback**: any `fg_key_*` returning -1 on a missing key must fall back to nothing rather than to zero -
   a zero default would load a model with silently wrong parameters;
4. **cost**: the port parses the same header the native reader does; the window is already read for the gate, so the
   added cost is the parse itself, which must be measured on load time, not assumed.

## Rollback

The conversion is behind nothing but itself: reverting `llama-model-loader.cpp` restores the native context. The
gate, the doors and the module stay as they are, so a rollback loses no evidence.
