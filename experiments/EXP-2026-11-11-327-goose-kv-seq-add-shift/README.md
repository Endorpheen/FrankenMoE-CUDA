# EXP327: KV sequence shift in Goose

`llama_kv_cache::seq_add(seq_id, p0, p1, shift)` — the K-shift path (P5-01/P5-09).

## Verdict

**GATED, NOT INTEGRATED** — the decision sequence, the per-cell order and the head rule
are exact; the source tree is read-only. No performance claim.

## Scope transferred

Four early exits before any cell is touched: the cache mirrors another one, the shift is
zero, the resolved range is empty, and — per cell — the position is outside the range or
the cell does not hold this sequence.

A cell shifted below zero is freed: its sequence bits go away and position and shift
reset. The head becomes the first such cell; when nothing was freed it becomes **zero**,
not the cache size and not the previous value.

Within a shifted cell the order is fixed: unlink the per-sequence position index, move the
position, relink unless the cell was freed.

## Gate

| check | result |
|---|---|
| 4 000 draws, 1–12 cells, negative and out-of-range p0/p1, zero and negative shifts | 17 656 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| a zero shift still walks the cells | 964 |
| a mirrored cache is shifted | 558 |
| the range end is inclusive | 757 |
| cells without the sequence are shifted | 7 058 |
| a cell that lands on zero is freed | 241 |
| the head always resets to zero | 170 |
| the head keeps the cache size when nothing is freed | 2 482 |
| the per-sequence index is relinked before the shift is emitted | 4 850 |

All eight are caught. The two that matter most are the ones the eye skips: `p < p1` being
exclusive, and the head falling back to zero rather than staying at the cache size.

## Files

- `source/host.goose` — the shift plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
