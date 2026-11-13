# EXP311: Hybrid indexed memory state ordering in Goose

`llama_memory_hybrid_idx::{state_write, state_read, state_drop}` and the ubatch
stream count (P5-05).

## Verdict

**GATED, NOT INTEGRATED** — the ordering and transaction are exact; the source
tree is read-only. No performance claim.

## Scope transferred

- **Write**: the hybrid base section first, the indexer section last, so it is a
  pure suffix — an older reader stops early instead of misparsing it.
- **Restore**, mirroring the write order: attention first (it returns the layout
  the indexer adopts, because two independent cell searches agree only while both
  caches see the same occupancy), then the recurrent part, then the indexer; a
  restore that rewrote cells invalidates the pool layout.
- **Failure transaction**: any failure drops the sequence from all three caches —
  the one state they agree on — and then propagates.
- **`state_drop`** clears directly rather than through `seq_rm`, because the
  recurrent cache may refuse and then only the other two would be cleared; the
  indexer clear invalidates the layout too.
- **Stream count**: `s1 - s0 + 1` per ubatch slot info.

## Gate

| check | result |
|---|---|
| write section order | 200 checks |
| restore event order across all failure combinations | 16 000 checks |
| drop order, with and without an indexer | 5 000 checks |
| stream counts, 1 000 draws | 1 090 checks |
| total | 22 290 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| write puts the indexer first | 200 |
| restore forgets to record the attention section | 12 304 |
| restore reads the recurrent part before attention | 779 |
| indexer restore does not invalidate the layout | 1 328 |
| failed restore does not drop the sequence | 1 016 |
| dropping all records nothing | 215 |
| drop forgets to invalidate the layout | 794 |
| stream count drops the inclusive one | 3 454 |

All eight are caught.

The drop-all mutation needed a fixture change to be visible: the output buffer was
not cleared between calls, so "records nothing" read back the previous call's
value. The fixture now fills the buffer with a sentinel and the clear-all event
writes 7, so silence and a correct event are different results.

## Files

- `source/host.goose` — the ordering plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
