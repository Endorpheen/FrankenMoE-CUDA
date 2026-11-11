# EXP332: KV ubatch application in Goose

`llama_kv_cache::apply_ubatch` for the plain (non-hybrid) cache.

## Verdict

**GATED, NOT INTEGRATED** — cell writes, the extension rule, the purge and the head move
are exact, including the resulting cell state; the source tree is read-only.

## Scope transferred

Per cell: an occupied destination is emptied first and the **highest** position it held is
recorded per sequence; then the new position, then the extension, then the batch's
sequences.

The extension rule: an embedding batch has no token ids, so the token is stored only when
the batch has one and the entry is not an embedding; for PLE the missing id is filled from
the image token, and from EOS when no image token is configured. 2-D positions are
transposed relative to the token index — y then x.

The purge: for every sequence whose position was overwritten, everything below that
position is removed, because native keeps the invariant that all positions between the
sequence minimum and maximum exist. Range end is exclusive.

The head moves **past** the last slot of the batch, not onto it.

## Gate

| check | result |
|---|---|
| 3 000 draws over 1–2 streams, 1–3 slots, both extension paths, all flags | 154 235 checks / 0 failures |

The gate compares records **and** the resulting cell state (position, sequence mask,
extension) and heads. Getting there needed three invalid-input classes removed from the
generator, all of which native rejects with asserts rather than handling: a slot count
larger than the stream's cells, duplicate sequence ids inside one token, and writing into a
cell that already holds two sequences. A fourth, out-of-range slot indices, showed up as
heap corruption rather than a mismatch — worth remembering that the first symptom was not a
failed comparison.

## Mutations

| mutation | failures |
|---|---:|
| the overwritten position recorded is the lowest, not the highest | 18 531 |
| an occupied destination is not emptied | abort (the run then breaks the seq_add precondition) |
| the extension is written only for 2-D positions | 15 362 |
| mixed embeddings keep their token id | 2 136 |
| the PLE fallback prefers the image token | 3 854 |
| the purge is skipped | 18 531 |
| only positions strictly above the minimum are purged | 1 783 |
| the head stops at the last slot instead of past it | 9 020 |

All eight are caught; one is caught by the run aborting, which is still detection.

## Files

- `source/host.goose` — the application plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
