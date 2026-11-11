# EXP291: KV range operations in Goose

The `CP/idxs` remainder of P5-01: `llama_kv_cache::seq_rm(seq_id, p0, p1)` in both
branches, `llama_kv_cache::seq_keep(seq_id)` and the same-stream branch of
`llama_kv_cache::seq_cp`.

## Verdict

**GATED, NOT INTEGRATED** — exact against a transcription of the class, but the
source tree is read-only, so nothing is wired into `llama-kv-cache.cpp`. No
performance claim.

## Scope transferred

All three are cell loops with a head update: the first cell freed during the
pass becomes the new search head when it lies before the current one.

- `seq_rm(seq_id, p0, p1)`: clamp `p0` to 0 and `p1` to `INT32_MAX`, walk the
  cells of the sequence's stream, and for cells inside `[p0, p1)` carrying the
  bit, drop the bit through the cell-level `seq_rm`.
- `seq_rm(-1, p0, p1)`: same walk over every stream, but each cell inside the
  range is cleared whole.
- `seq_keep(seq_id)`: one bit survives; a cell that does not carry it is
  released, and the first released cell moves the head back.
- `seq_cp(src, dst, p0, p1)` inside one stream: no data copy, only metadata —
  for cells inside the range that carry `src`, add `dst`.

The cell-level helpers (`seq_rm`, `seq_keep`, `seq_add`, `rm`) and the ordered
`seq_pos` array come from the same arena design as EXP199 and EXP289.

Cross-stream `seq_cp` is deliberately out of scope: it enqueues a buffer copy in
`sc_info` and rewrites the destination stream's cells from the source, which is
buffer work rather than cell metadata. It is recorded as open, not approximated.

## Gate

The oracle is a transcription of `llama_kv_cache` over a model that keeps the
native `std::set` per sequence, two streams, random occupancy (1–2 sequences per
cell), and 40 operations per trial drawn from all four entry points. After every
step the gate compares positions, shifts, used flags, all three ext fields, the
sequence words, the full `seq_pos` contents and counts, and both heads.

| check | result |
|---|---|
| 120 trials × 40 operations, full state after each | 3 937 168 checks / 0 failures |

## Mutations

| mutation | detection |
|---|---:|
| range test includes the upper bound | 2 431 |
| `seq_rm` forgets to move the head | 166 |
| any-sequence rm skips the last stream | abort (the skipped stream keeps stale state) |
| `seq_keep` leaves the old sequence pairs behind | 12 824 |
| `seq_keep` keeps the ext block on release | 188 692 |
| `seq_cp` no longer clamps a negative `p0` | 2 795 |
| `seq_cp` no longer opens a negative `p1` | abort (fails the destination precondition on the next add) |
| head update moves backwards | 8 142 |

All eight are caught.

One precondition had to be modelled rather than assumed: native `seq_add`
asserts that the destination sequence bit is absent, and `seq_cp` calls it
unconditionally. A random state where a cell already carries both sequences is
therefore outside the defined behaviour. The gate skips such a draw and the
module asserts the precondition explicitly, so neither side tests an undefined
state — and the duplicate-pair mismatch that first appeared (14 pairs against 12)
was exactly this, not a defect in the port.

## Files

- `source/host.goose` — the port plus a one-entry dispatcher.
- `tests/fixture.cpp` — native transcription, random driver, per-step comparison.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
