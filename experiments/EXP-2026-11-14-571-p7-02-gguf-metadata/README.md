# EXP-2026-11-14-571 — GGUF context and metadata (P7-02 S1)

Native counterpart: `ggml/src/gguf.cpp`, the context and mutation block
(`gguf_init_empty`, the KV list, all setters, `gguf_remove_key`, `gguf_set_kv`,
the reserved-key rule). Base commit: `73c51a5e` (inventory was made at
`6395755a`).

## What this slice covers

`source/gguf-metadata.goose` keeps a flat KV list: per entry a key offset and
length, the type id, an array flag (0 scalar, 1 value array, 2 string array),
raw scalar bits or a pool offset, the element count and the byte length. Keys,
strings and array bytes live in one caller-owned pool; state is the entry count
and the pool cursor.

Exported entry points: `s1_init`, `s1_set_val`, `s1_set_str`, `s1_set_arr`,
`s1_set_arr_str`, `s1_remove`, `s1_set_kv`.

Semantics copied from the native code:

- a setter removes an existing key first and appends, so a replacement moves to
  the end;
- `remove` returns the old index, or -1 when the key is absent;
- `set_kv` walks the source in order through the same setters, so destination
  duplicates are removed and the resulting order is source order;
- `general.alignment` accepts only a u32 power of two; native aborts on other
  types/values, the port returns -1 so the harness can assert the refusal.

The writer (S2), tensor data (S3), split/merge (S4) and `llama_model_saver`
(S5) are out of this slice; see `docs/goose-migration/P7-02-COVERAGE.md`.

## Gate

`tests/build.sh` builds the Goose file with `--standalone` and runs two hosts
over the same 20 cases:

- `tests/probe_s1.cpp` — the real `gguf_set_*` from the prebuilt
  `libggml-base`, printing a canonical TSV;
- `tests/host_s1.cpp` — the Goose port, printing the same TSV.

`diff` of the two files is empty: 46 records compared, 0 failures.

```
FG_G1 cases=46 values=46 failures=0
```

No weights and no GPU: the native oracle creates a fresh `gguf_context` per
case. `FG_LLAMA`/`FG_BIN` select another llama.cpp base.

## Mutations

`tests/mutate_s1.py` holds 12 mutations in two halves; each rewrites the port,
regenerates the C, rebuilds the host and reruns the TSV comparison.

```
python3 tests/mutate_s1.py --half 1   # 6/6 caught
python3 tests/mutate_s1.py --half 2   # 6/6 caught
```

## Recorded limits

- Getter coverage is limited to what the fixture reads; the typed getter block
  is exercised through the writer path in S2.
- The gate runs at `-O1` for the mutation host and `-O0` for the generated
  object, matching the experiment template.
- `tests/oracle_s1.tsv` is the recorded oracle output from the author's run; the
  gate regenerates it on every build and compares against the Goose side.
