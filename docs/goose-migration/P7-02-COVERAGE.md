# P7-02 coverage — GGUF writing, splitting and tensor serialization

Updated: 2026-11-14. Base commit for the inventory: `6395755a`.
Native tree: `upstream/llama.cpp`, cross-checked with the flint build.

The requirement owns five independent slices. S1 is accepted and canonical; the
rest stays open. The reader side was already ported earlier and is not counted
here.

## Slice table

| slice | native scope | lines | state |
|---|---|---:|---|
| S1. Context and metadata | `gguf_init_empty`, KV list and lookup, all `gguf_set_val_*`, `gguf_set_arr_data`, `gguf_set_arr_str`, `gguf_set_kv`, `gguf_remove_key`, the reserved-key rule | ~450 | **done (EXP571)** |
| S2. Writer core | writer base/buf/file, magic/version/counts, KV encoding order, tensor descriptors, alignment and padding, `only_meta`, `meta_size`/`meta_data`, `write_to_file` | ~280 | **done (EXP596)** — no tensor descriptors yet |
| S3. Tensor descriptors and data | `gguf_add_tensor`, `set_tensor_type`, `set_tensor_data`, duplicate names, offset accumulation, data read-back, `llama-io` string helper | ~180 | open |
| S4. Split and merge | `split_params`, `split_strategy`, `should_split`, `gguf_split`, `gguf_merge`, metadata replay, delete/dry-run | 613 | open |
| S5. Model saver | `llama_model_saver` add_kv_from_model, vocab/hparams serialization, tensor enumeration, save | 572 | open |

## S1 — accepted

Source: `experiments/EXP-2026-11-14-571-p7-02-gguf-metadata/`;
canonical file `goose/model/gguf-metadata.goose`.

`gguf_context` is a flat KV list: one record per entry (key offset/length, type,
array flag, raw scalar bits or pool offset, element count, byte length), with
keys, strings and array bytes in a caller-owned pool. The port keeps the native
mutation semantics:

- a setter removes an existing key first and appends, so a replacement moves to
  the end of the list (`gguf_remove_key` + `emplace_back`);
- `remove` returns the old index or -1 when the key is absent;
- `set_kv` walks the source in order through the same setters, so destination
  duplicates disappear and the order is source order;
- `general.alignment` accepts only a u32 power of two; native aborts, the port
  returns -1 so a host can assert the refusal without dying.

Gate: the native `gguf_set_*` path and the Goose host run the same 20 cases and
print one canonical TSV each; `diff` is empty — **46 compared records, 0
failures**. Mutations: **12/12** in two halves of 6 (set replacement, remove
shift, missing-key result, reserved type/value, key length, scalar type, string
length, array count, `set_kv` count/replacement/bytes, string-array flag).

## Open boundaries

- The writer (S2) and tensor data (S3) are native; no byte-exact GGUF output
  exists yet, so any "saved model" claim is premature.
- Split/merge (S4) still runs natively; the existing `shards.goose` covers only
  naming and validation rules from `src/llama.cpp`, not the tool.
- `llama_model_saver` (S5) needs a loaded model or a synthetic
  hparams/vocab fixture; it is the only slice that cannot be gated
  weight-free as-is.
- `llama-io` length-prefixed strings are used by state/checkpoint code, not by
  `gguf_write_out`; they are assigned to S3 but may move out if the state
  serialization owner claims them.
- The tensor type/block-size table stays native for now; a writer port must
  either depend on it or port it.
