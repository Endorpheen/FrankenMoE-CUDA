# EXP341: Hybrid indexed operations in Goose

`llama_memory_hybrid_idx` delegates, staleness marking, and the two state halves.

## Verdict

**GATED, NOT INTEGRATED** — order, refusal and staleness rules are exact; the source tree
is read-only.

## Scope transferred

Staleness: positions are clamped at zero, a negative id applies to every sequence, and the
stored value keeps the **minimum** edited position. `stale_pos` returns zero — "regroup the
whole sequence" — when the edit is at or below the sequence's first position.

- `seq_rm`: recurrent first because it can refuse, and a refusal stops everything before the
  indexer or attention see anything. On success the indexer is told the position, then
  removed, then told it again — the first read happens before the removal.
- `seq_cp`: whole sequences only (precondition `p0 <= 0 && p1 < 0`), destination marked from
  zero because it inherits the source's pooled rows.
- `seq_keep`: **every** sequence marked from zero, since every other sequence loses cells.
- `seq_add`: marked at `p0 + shift` for a negative shift, at `p0` otherwise.
- `seq_div`: marked from zero.
- `state_write`: the indexer section is a pure suffix.
- `state_read`: attention layout reused instead of searched, stale mark only when the copied
  layout is non-empty, and any throw drops the sequence from all three caches.
- `state_drop`: deliberately not via `seq_rm`, so a refusing recurrent cache cannot prevent
  the other two from being cleared.

## Gate

| check | result |
|---|---|
| 5 000 draws over all eight operations, both stale extremes and forced boundary cases | 44 122 checks / 0 failures |

**The first gate was comparing the port against itself.** The staleness model was mutated
while the expected records were being built, and the mutated model was then handed to the
port as its *input* — so the port received the post-operation state and three mutations
passed at 0. The fix is one line (`model_before` copied before the switch), after which all
of them are caught. Worth stating plainly: a gate that builds its input from its own
expectation cannot fail, and it looks exactly like a passing gate.

Two other first attempts were also unusable and were replaced: a mutation that searched for
an anchor that no longer existed after the model rewrite, and one that only tested
out-of-range clamping, which the operation paths never exercise.

## Mutations

| mutation | failures |
|---|---:|
| a refusing recurrent cache still touches the indexer | 461 |
| the indexer is removed before being told the position | 452 |
| stale_pos never zeroes at the first position | 356 |
| a negative shift is not accounted for | 46 |
| seq_keep marks only the kept sequence | 950 |
| the minimum is overwritten instead of kept | 128 |
| the indexer section is written first | 933 |
| a half-failed restore is left as it is | 309 |
| an empty copied layout still invalidates the pools | 423 |

All nine are caught.

## Files

- `source/host.goose` — the delegates and state halves.
- `tests/fixture.cpp` — comparison driver with an explicit pre-operation model copy.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
