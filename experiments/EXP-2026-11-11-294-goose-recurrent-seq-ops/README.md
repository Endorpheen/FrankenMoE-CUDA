# EXP294: Recurrent sequence operations in Goose

`llama_memory_recurrent::seq_cp`, `seq_keep`, `seq_add` and `seq_div` — the
per-sequence half of the recurrent memory subsystem (P5-04), after EXP293's
`find_slot`.

## Verdict

**GATED, NOT INTEGRATED** — exact against a transcription of the class; the
source tree is read-only. No performance claim.

## Scope transferred

A recurrent cache keeps one cell per sequence, so these operations act on a
sequence's cell rather than on a range of cells. The position bounds are still
clamped and still select the cell; they do not walk the array.

- `seq_cp(src, dst)`: drop `dst` from its current cell (releasing it and
  decrementing `used` when it empties), then add `dst` to `src`'s cell and point
  `tail[dst]` at it. Positions are clamped but never used — the native code
  clamps them and then does not read them, and the port keeps that shape rather
  than inventing a range test.
- `seq_keep(seq_id)`: every cell loses its tail unless its index equals
  `seq_id`; cells without the sequence are released, counting `used` down; the
  survivor keeps only that sequence; the first freed cell becomes the head when
  it lies before the current one.
- `seq_add(seq_id, p0, p1, shift)`: no-op for `shift == 0`, empty range, or an
  out-of-range sequence; otherwise the sequence's own cell moves by `shift` when
  its position lies in `[p0, p1)`.
- `seq_div(seq_id, p0, p1, d)`: the same shape with `d == 1` as the no-op and
  truncating integer division.

## Gate

Oracle: a transcription of the four methods over a model with the native
`std::set`, driven by 300 states × 30 random operations. After every operation
the gate compares every cell's position, `src`, `tail` and sequence set, plus
`head` and `used`.

| check | result |
|---|---|
| 300 × 30 operations, full cell array after each | 1 170 000 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| `seq_cp` ignores cell zero | 241 |
| `seq_cp` keeps the destination bit | 2 057 |
| `seq_cp` does not move the destination tail | 1 003 |
| `seq_cp` forgets to drop the used count | 2 838 |
| `seq_keep` leaves the other tails in place | 46 255 |
| `seq_keep` does not move the head | 7 754 |
| range test includes `p0` | 9 |
| `seq_div` divides by one more | 269 |

All eight are caught. The narrowest margin is the range boundary: it only differs
when a sequence's cell sits exactly at `p0`, and the generator has to draw that
position for the mutation to be visible at all — which is why the probe range
spans negative, zero and beyond the cell positions rather than a uniform window.

## Files

- `source/host.goose` — the port plus a one-entry dispatcher.
- `tests/fixture.cpp` — native transcription, random driver, comparison.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
