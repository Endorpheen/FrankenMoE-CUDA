# EXP292: Cross-stream `seq_cp` in Goose

The stream-copy branch of `llama_kv_cache::seq_cp`, the part EXP291 left open.

## Verdict

**GATED, NOT INTEGRATED** — exact against a transcription of the class; the
source tree is read-only, so nothing is wired in. No performance claim.

## Scope transferred

When source and destination live in different streams, metadata alone is not
enough — the destination stream's cells are rebuilt from the source:

1. the full-buffer test (`p0`/`p1` inside the cache are rejected, native asserts
   here and so does the port),
2. the copy pair the caller has to queue (native pushes it into `sc_info`; the
   port reports it instead of owning a buffer queue),
3. `v_cells[dst].reset()` — positions −1, ext and shift zeroed, words zeroed,
   every sequence count zeroed, `has_shift` cleared,
4. for every source cell carrying the sequence: undo the shift to recover the
   real position, `pos_set`, `seq_add` for the destination, re-apply the shift
   through `pos_add` (which keeps the ordered sequence array consistent), then
   copy the ext block,
5. `v_heads[dst] = v_heads[src]`.

Only the buffer copy itself is not ported — that is the queued `sc_info` entry,
and the port reports its two values.

## Gate

The oracle is a transcription over a model with the real `std::set`. Both
streams are randomly occupied with 1–2 sequences per cell and a non-zero shift,
and the copy pair is drawn from all five boundary positions so the full-buffer
test is exercised in the accepted direction.

| check | result |
|---|---|
| 150 random states, whole destination stream compared | 130 370 checks / 0 failures |

Compared per trial: both cells' positions, shifts, used flags, all three ext
fields, sequence words, full `seq_pos` contents and counts, both heads, and the
reported copy pair.

## Mutations

| mutation | detection |
|---|---:|
| restored position adds the shift | 807 |
| copied cell is not marked used | 465 |
| initial position is off by one | 102 |
| shift is not re-applied after the copy | 755 |
| reset forgets the sequence counts | 1 756 |
| reset keeps the ext block | 6 700 |
| destination head is left alone | 125 |
| copy filters on the destination sequence | 6 849 |
| full-buffer test is off by one | abort (native asserts, the port asserts) |

All nine are caught.

The rejected-buffer direction is an assert in native code, not a returned error,
so the port asserts too and the gate covers only the accepted direction; a
mutation that widens the rejection turns a valid state into an abort, which the
driver counts as detection.

## Files

- `source/host.goose` — the port plus a one-entry dispatcher.
- `tests/fixture.cpp` — native transcription, random driver, comparison.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
