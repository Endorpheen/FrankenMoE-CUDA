# EXP321: KV state ranges in Goose

Which cells `llama_kv_cache::state_write` puts into a stream (P5-08).

## Verdict

**GATED, NOT INTEGRATED** — the range selection is exact; the source tree is
read-only. No performance claim. The I/O itself stays native.

## Scope transferred

A cell is written when it is not empty, when it carries the requested sequence (all
cells when the sequence is -1), and — only for a named sequence — when the sliding
window at that sequence's highest position does not exclude it. Consecutive cells are
coalesced into ranges, and the per-stream counts must sum to the range lengths.

The partial-only flag drops the whole section for a cache without a sliding window.

## Gate

| check | result |
|---|---|
| 3 000 draws over streams, sequence ids including -1, all four SWA types | 40 670 checks / 0 failures |

The gate checks each stream's count, every range boundary, and the invariant that the
counts equal the sum of the range lengths — the same invariant the native code asserts.

## Mutations

| mutation | failures |
|---|---:|
| empty cells are written | 7 332 |
| the sequence bit is not checked | 23 113 |
| the sliding window is not checked | 13 873 |
| a range remembers the wrong start | 27 870 |
| the last range ends one cell early | 1 306 |
| the per-stream count is not incremented | 7 196 |
| partial-only never drops a section | 202 |
| the sliding window also applies to all sequences | 5 119 |

All eight are caught.

## Files

- `source/host.goose` — the selection plus a one-entry dispatcher.
- `tests/fixture.cpp` — transcription and comparison.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
