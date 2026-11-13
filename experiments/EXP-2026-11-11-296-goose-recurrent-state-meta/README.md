# EXP296: Recurrent state metadata in Goose

`llama_memory_recurrent::state_write_meta` and the whole-cache branch of
`state_read_meta` (P5-04 serialization).

## Verdict

**GATED, NOT INTEGRATED** — exact against a transcription; the source tree is
read-only. No performance claim. The tensor payloads around the metadata
(`state_write_data` / `state_read_data`) stay native: they move ggml tensor rows,
not cell metadata.

## Scope transferred

The metadata stream is a flat run of 32-bit values — `pos`, `n_seq_id`, then that
many sequence ids — for every cell in every given range.

Writing: `n_seq_id` is the cell's sequence count only when the whole cache is
written (`seq_id == -1`) and zero for the single-sequence form, which is exactly
what makes the two forms distinguishable on read.

Reading the whole cache: capacity check, full clear, then per cell read `pos` and
`n_seq_id`; reject an out-of-range id; reject a sequence that already has a tail;
record the cell as that sequence's tail. On success `head = 0`, `used =
cell_count`, and every restored cell gets `src = cell_id` so the recurrent state
survives the restore.

The single-sequence branch of `state_read_meta` calls `find_slot`, which EXP293
already covers as its own module; it is not duplicated here. The gate asserts
that only the whole-cache branch is exercised.

## Gate

The oracle is a transcription writing into its own byte buffer, so the write side
is compared byte for byte, and the read side is fed the exact bytes the
transcription produced.

| check | result |
|---|---|
| 2 000 trials: stream length, stream bytes, then restored cells + head/used | 44 480 checks / 0 failures |
| rejected streams (duplicate tail, out-of-range id, over-capacity) | compared by result, not skipped |

The generator injects the two rejected cases on purpose — a duplicate sequence id
and an id beyond `n_seq_max` — and every sixteenth trial is kept completely valid,
so the exact-capacity boundary (`cell_count == size`) is exercised with a stream
that native accepts.

## Mutations

| mutation | failures |
|---|---:|
| write drops the sequence count | 856 |
| write skips every second sequence id | 666 |
| read rejects an exactly full cache | 6 460 |
| read accepts a duplicate tail | 122 |
| read accepts a far out-of-range sequence id | 23 |
| read leaves the used count at zero | 98 |
| read does not stage the source index | 1 606 |
| read forgets the sequence bit | 522 |

All eight are caught.

One mutation was invisible until the generator changed: with the rejected cases
injected everywhere, no trial ever produced a valid stream at `cell_count ==
size`, so the "exactly full cache" mutation had nothing to disagree with. Every
sixteenth trial now draws a clean stream, and that mutation fails 6 460 checks.

## Files

- `source/host.goose` — the port plus a one-entry dispatcher.
- `tests/fixture.cpp` — native transcription, byte-buffer oracle, comparison.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
