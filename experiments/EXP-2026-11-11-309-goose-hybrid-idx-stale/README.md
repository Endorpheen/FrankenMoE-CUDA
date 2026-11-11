# EXP309: Hybrid indexed memory pool staleness in Goose

The staleness rules of `llama_memory_hybrid_idx` (P5-05). A pooled key is valid only
while the grouping that produced it holds; grouping is sequence relative, so an
edit at `p0` leaves every pool that ends before `p0` alone.

## Verdict

**GATED, NOT INTEGRATED** — the rules are exact; the source tree is read-only. No
performance claim.

## Scope transferred

- **Marking** keeps the *minimum* edited position, clamped at zero; a negative
  sequence id marks every sequence.
- **`stale_pos`** returns zero — regrouping the whole sequence — when the edit is
  at or below the sequence's first position, and `p0` otherwise.
- **Per-operation decisions**: `seq_rm` tries the recurrent cache first and stops
  entirely if it refuses, marking with `stale_pos(p0)`; `seq_add` marks with
  `p0 + shift` when the shift is negative, because the moved cells end below `p0`;
  `seq_cp` marks the destination from zero; `seq_keep` marks every sequence;
  `seq_div` marks the sequence from zero.
- Without an indexer memory (`mem_idx == nullptr`) nothing is marked.

## Gate

| check | result |
|---|---|
| marking, 2 000 draws including negative ids | 12 000 checks |
| `stale_pos`, 1 000 draws | 1 000 checks |
| `seq_rm` chain including a recurrent refusal | 1 500 checks |
| `seq_add` with negative shifts | 1 000 checks |
| `seq_cp` / `seq_keep` / `seq_div` marks | 3 862 checks |
| total | 19 362 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| mark-all keeps the larger position | 3 180 |
| single-sequence mark overwrites instead of minimising | 871 |
| first-position test is exclusive | 44 |
| negative sequence does not regroup from zero | 256 |
| recurrent refusal does not stop the chain | 383 |
| negative shift marks the unshifted position | 37 |
| `seq_cp` does not mark the destination | 460 |
| `seq_keep` marks only one sequence | 1 197 |

All eight are caught. The two narrow margins are the boundary cases: an edit exactly
at the first position (44) and a shift that moves cells below it (37) — both are
single-draw conditions, which is why the generator draws `p0` and `seq_min` from
overlapping ranges instead of one wide window.

## Files

- `source/host.goose` — the rules plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
