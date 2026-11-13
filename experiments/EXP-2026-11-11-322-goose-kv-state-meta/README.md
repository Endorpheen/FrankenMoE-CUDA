# EXP322: KV state metadata in Goose

`llama_kv_cache::state_write_meta` (P5-08).

## Verdict

**GATED, NOT INTEGRATED** — the record layout is exact; the source tree is
read-only. No performance claim. The I/O stays native.

## Scope transferred

For every cell in the selected ranges the stream carries: position, the number of
sequence ids, the cell extension when the cache has one, and those ids in increasing
order. A named sequence writes only itself when the cell carries it; an unnamed write
carries every sequence the cell holds. The order matters — the reader re-inserts ids
in the order they appear.

## Gate

| check | result |
|---|---|
| 3 000 draws over sequence counts, named/unnamed writes, with and without extension | 109 429 checks / 0 failures |

The gate compares the whole stream slot by slot, so a missing extension field or a
shifted id list fails immediately rather than at the reader.

## Mutations

| mutation | failures |
|---|---:|
| a named sequence is not matched | 55 653 |
| an unnamed write carries nothing | 29 218 |
| the cell extension is not written | 38 031 |
| each written id is shifted by one | 17 170 |
| the position is not written | 23 873 |
| the id count ignores the filter | 13 658 |
| range ends are inclusive | 62 314 |
| ids start where the position is | 66 820 |

All eight are caught.

One candidate mutation was rejected as unobservable rather than missed: reversing the
word index used to test the sequence bit changes nothing while the cache has a single
bit word, which is the case throughout this gate. The replacement (each id shifted by
one) fails 17 170 checks.

## Files

- `source/host.goose` — the layout plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
