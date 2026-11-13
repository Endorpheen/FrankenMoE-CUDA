# EXP305: DSV4 compressed-state operations in Goose

`llama_dsv4_comp_state::{clear, seq_cp, apply_copies}` plus the stream-range
helpers and the stream ids `state_write` computes. P5-03.

## Verdict

**GATED, NOT INTEGRATED** — exact against a transcription; the source tree is
read-only. No performance claim. The tensor copies themselves stay native.

## Scope transferred

The compressed state holds one plane per stream and rollback depth: plane
`d*n_stream + stream` for `d` in `[0, n_rs_seq]`.

- **`clear(seq_id, data)`** — nothing without `data`; for one sequence, its column
  in every depth; for `seq_id < 0`, every plane.
- **`seq_cp(src, dst)`** — a self-copy is a no-op; otherwise the destination
  column is cleared and the pair is queued.
- **`apply_copies`** — each queued source column is copied onto its destination in
  queue order.
- **`src_stream_range` / `dst_stream_range`** — the native bounds and the two
  mismatch rejections.
- **`state_write` stream ids** — `rs_idx[seq]*n_stream + s0 + s`, with the
  rejection when the rollback index is out of range or the sequence id exceeds
  the table.

Native throws on every rejection; the FFI contract is `-1`, stated and pinned.

## Gate

| check | result |
|---|---|
| source/destination stream ranges, 4 000 draws | 6 000 checks |
| clear column and clear-all plane sets | 1 000 checks |
| `seq_cp` decision and queue pair | 1 000 checks |
| apply order | 1 500 checks |
| write stream ids, 2 000 draws | 8 280 checks |
| total | 17 780 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| sequence stream range widens to two streams | 949 |
| whole-state range collapses to one stream | 265 |
| destination range accepts a mismatched stream count | 719 |
| clear column ids drop the stream stride | 427 |
| clear-all plane count loses one depth | 96 |
| copying a sequence onto itself still queues | 461 |
| apply copies swap source and destination | 2 978 |
| write ids reject the deepest legal rollback | 1 327 |

All eight are caught.

Two harness slip-ups, both mine: the dispatcher argument `op` was never written
into the packet, so every operation ran as op 0 and the destination-range checks
compared results from the wrong function; and the apply records were written at
`2*i*8` through a helper that already scales by 8, so they landed past the buffer.
The second is the same double-scaling as EXP297 — **a helper that takes an index
must never receive a byte-scaled offset**, and that is now written into HANDOFF as
a rule rather than a one-off note.

One candidate mutation was rejected as equivalent, not missed: `rb > n` and
`rb >= n+1` are the same predicate over integers. The replacement rejects the
deepest legal index and fails 1 327 values.

## Files

- `source/host.goose` — the operations plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
