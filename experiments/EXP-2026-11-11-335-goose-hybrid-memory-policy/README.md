# EXP335: Hybrid memory policy in Goose

`llama_memory_hybrid` — split selection, prepare order, delegation order and the refusals
(P5-05/P5-09).

## Verdict

**GATED, NOT INTEGRATED** — every order and every refusal rule is exact; the source tree
is read-only.

## Scope transferred

| call | rule |
|---|---|
| `init_batch` | all-output splits by sequence, otherwise equally with the unified flag set only when the attention cache is *not* unified, and the rollback window `1 + n_rs_seq` when the recurrent cache needs one |
| `init_batch` prepare | recurrent first; its failure is a failed context and attention is never asked |
| `seq_rm` | recurrent first **because it can refuse without mutating**; attention only after |
| `seq_cp` / `keep` / `add` / `div` / `clear` / state halves | attention then recurrent |
| `seq_pos_min` / `max` | total min is the **larger** of the two minima, total max the **smaller** of the two maxima, so a cache without the sequence (-1) does not hide the other |
| `state_read` | if recurrent throws after attention succeeded, attention is cleared for that sequence — the restore is a transaction |
| `memory_breakdown` | merge by backend key, summing, and **insert** recurrent-only keys |

## Gate

| check | result |
|---|---|
| 8 500 draws across all six operations | 37 928 checks / 0 failures |

Two generator defects were found by the gate itself and fixed rather than tolerated: both
breakdown sides are maps, so keys are unique **within a side** while the two sides may
overlap — the first generator produced duplicate attention keys and made the port look
wrong when the input was impossible. The port also double-counted the first recurrent row
for a recurrent-only key, which the union case exposed only after key overlap was added.

## Mutations

| mutation | failures |
|---|---:|
| an all-output batch splits equally | 6 400 |
| the unified flag is inverted | 1 541 |
| the rollback window is one short | 1 022 |
| attention prepares before recurrent | 4 529 |
| a refusing recurrent cache still runs attention | 2 042 |
| the mutation pair runs recurrent first | 500 |
| a failed restore leaves attention populated | 472 |
| the total minimum takes the smaller of the two | 3 790 |
| the breakdown forgets recurrent-only keys | 931 |

All nine are caught.

## Files

- `source/host.goose` — the policy plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
