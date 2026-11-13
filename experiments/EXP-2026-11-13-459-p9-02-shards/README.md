# EXP459: shard naming, shard validation and endian guard in Goose (P9-02 third slice)

Multi-shard models are loaded by opening the first file and then reconstructing
the names of the remaining shards. This experiment moves that naming and the
shard checks into Goose.

## What was ported

`source/shards.goose`, from `src/llama.cpp` (`llama_split_path`) and
`src/llama-model-loader.cpp` (the `n_split > 1` block):

| decision | rule |
|---|---|
| shard path | `"<prefix>-%05d-of-%05d.gguf"` with `split_no + 1`; `%05d` pads to five digits and prints more when the number needs them |
| capacity | the call fails (returns 0) when the result plus its terminator does not fit, matching `snprintf` |
| first shard | the model must be opened on split 0 |
| split count | the listed splits must match `split.count` |
| shard index | each additional file must declare `split.no` equal to its position |
| tensor total | the accumulated tensor count must equal `split.tensors.count` |
| endian guard | only the little-endian magic `0x46554747` and GGUF v2/v3 are accepted; native `gguf.cpp` has no byte-swap path, so a big-endian file is rejected there too |

The decimal formatting, the terminator and the capacity check are Goose; the file
open, the name map and the duplicate-name rejection stay native.

## Gate

`tests/shards_fixture.cpp` transcribes `llama_split_path` with `snprintf` and the
loader checks, then compares both arms.

| corpus | cases |
|---|---|
| digit width and decimal length (0 .. 1 000 000) | 24 |
| paths: 5 prefixes x 7 counts x up to 4 shard numbers, byte-exact including the terminator | 180 |
| capacity at exactly the result length and one byte less | 23 |
| shard index / split count / tensor total matrices, first-shard guard | 78 |
| endian magic and version tables (little-endian, byte-swapped, v1..v5) | 11 |
| total | **234 cases, 406 values, 0 failures** |
| ASan + UBSan (module and fixture) | 0 failures, rc 0 |

Mutations 8/8 caught: padding width becomes four (282 failures), pointer not
one-based (110), capacity check ignores the terminator (runtime trap: Goose
bounds check), separator becomes a slash (110), first-shard check accepts any
index (2), a shared-body comparison becomes `<=` (10), big-endian magic accepted
(1), version 1 accepted (2).

## Findings while gating

- **The compiler shares identical function bodies.** `split_count_ok`,
  `shard_index_ok` and `tensor_count_ok` are the same `a == b` body and the
  emitted C contains one function used three times. The wrapper mapping therefore
  keys on call order, and a future divergence between those bodies would need its
  own call site.
- **`snprintf`'s contract includes the terminator.** The first version wrote only
  the visible bytes and the byte comparison failed; the module now writes the NUL
  and the capacity test rejects a buffer that fits the text but not the NUL. The
  mutation that drops that rule is caught by a Goose bounds trap, not by a
  mismatch.
- **`pad` is a reserved word**, as is `block`; the module uses `zeros`/`align_up`
  style names.

## Scope

P9-02 still needs mmap/pinned allocation. It stays IN_PROGRESS.

## Reproduce

    bash experiments/EXP-2026-11-13-459-p9-02-shards/build_fixture.sh
    python3 experiments/EXP-2026-11-13-459-p9-02-shards/mutate.py
