# EXP328: KV sequence division and position extremes

`llama_kv_cache::seq_div` plus `seq_pos_min` / `seq_pos_max` (P5-01).

## Verdict

**GATED, NOT INTEGRATED** — decisions, record order and sentinels are exact; the source
tree is read-only. No performance claim.

## Scope transferred

Division: a mirrored cache, `d == 1`, an empty range and per-cell mismatches all skip. A
divided cell keeps its sequence bits — unlike a shift it can never go negative — and its
shift grows by exactly the difference the position lost. The per-sequence index is
unlinked before the division and relinked after.

Extremes: a mirrored cache delegates and that result is taken as given. Otherwise the
extremes run over cells holding the sequence, and an empty sequence yields **-1** — the
same sentinel as an empty cell, which is why the answer cannot be read as "position zero".

## Gate

| check | result |
|---|---|
| 4 000 draws, division with d ∈ {1, 2, 3, 4, 5, 6}, both extremes per draw | 26 244 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| a division by one still walks the cells | 538 |
| a mirrored cache is divided | 370 |
| the shift gains the new position instead of the difference | 2 161 |
| the index is relinked before the division | 7 683 |
| the minimum is computed as a maximum | 3 024 |
| an empty sequence reports zero instead of nothing | 445 |
| the maximum ignores the sequence bits | 1 675 |
| a mirrored cache no longer delegates for the extremes | 531 |

All eight are caught.

One mutation was provably unobservable and replaced: "empty cells count as positions" in
the maximum. An empty cell holds -1, the running maximum starts far below that, and the
fallback is also -1, so the value is identical whether or not empty cells are filtered —
measured at 0 failures before replacement. The replacement drops the sequence-bit check
instead, which is observable, and it is caught at 1 675.

## Files

- `source/host.goose` — division and extremes plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
