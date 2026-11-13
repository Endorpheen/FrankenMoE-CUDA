# EXP336: Hybrid indexed memory initialisation in Goose

`llama_memory_hybrid_idx::{init_batch, init_update, clear}`.

## Verdict

**GATED, NOT INTEGRATED** — the layout rule and the clear order are exact; the source tree
is read-only.

## Scope transferred

`init_batch` repeats the hybrid rules — the split choice with the inverted unified flag and
the rollback window, recurrent prepare before attention — and adds the rule that matters
here: **the indexer takes the attention cache's slot layout verbatim**. It does not search
for its own cells; a separate search could return a different placement, and the indexer
keys would then point at cells the attention cache never wrote.

A failed batch yields no layout at all — never a partial one — and without an indexer
memory nothing is copied.

`init_update` carries the optimize flag into the context and decides nothing.

`clear` empties the base halves first, then the indexer half, and resets its staleness for
every sequence to zero.

## Gate

| check | result |
|---|---|
| init_batch, 4 000 draws including both failure points and a family without an indexer | 32 000 checks |
| init_update, 1 000 draws | 1 000 checks |
| clear, 2 000 draws with and without an indexer | 1 774 checks |
| total | 34 774 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| an all-output batch splits equally | 11 502 |
| the rollback window is one long | 1 281 |
| attention prepares before recurrent | 7 032 |
| the indexer layout is not copied | 1 842 |
| the indexer layout is copied even when there is no indexer | 1 838 |
| a partial batch still reports success | 2 230 |
| the optimize flag is dropped | 523 |
| the indexer staleness is not reset | 2 952 |

All eight are caught. The pair around the layout copy is what protects the rule from both
sides: copying nothing and copying unconditionally are both wrong.

## Files

- `source/host.goose` — the initialisation plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
