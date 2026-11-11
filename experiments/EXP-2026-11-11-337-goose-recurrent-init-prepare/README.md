# EXP337: Recurrent initialisation and preparation in Goose

`llama_memory_recurrent::{init_batch, prepare, init_update}`.

## Verdict

**GATED, NOT INTEGRATED** — the split choice and the dry-run transaction are exact; the
source tree is read-only.

## Scope transferred

`init_batch`: all-output splits by sequence; anything else splits **equally and
sequentially regardless of the unified flag** — the recurrent cache always splits
sequentially, unlike the attention cache where the flag matters. The rollback window is
`1 + n_rs_seq` and exists only when there are rollback sequences. The drain ends at the
zero-token ubatch, and unplaced tokens fail the batch.

`prepare` is a dry run: the state is small, so it is copied, every ubatch asks for a slot,
and the copy is restored — **also on success**. Preparation commits nothing; it only
answers whether the sequence could be placed. A refusal stops the walk at the first ubatch
that could not be placed and does not consult the rest.

`init_update` never updates: the cache reports "no update" rather than a context that would
have to do something.

## Gate

| check | result |
|---|---|
| init_batch, 4 000 draws with the terminating zero at 25% and both failure points | 30 022 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| an all-output batch splits equally | 11 440 |
| the split is not sequential | 4 000 |
| the rollback window is missing | 1 290 |
| unplaced tokens are accepted | 3 275 |
| the dry run commits nothing but reports nothing either | 9 171 |
| the walk continues past a refused slot | 624 |
| the state is only restored on failure | 2 210 |
| init_update returns a working context | 500 |

All eight are caught. "Restore only on failure" is the subtle one: the state is restored
even when preparation succeeds, because success means "could be placed", not "was placed".

## Files

- `source/host.goose` — the initialisation plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
