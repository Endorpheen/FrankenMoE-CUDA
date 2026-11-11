# EXP377: aligned-read arithmetic -- gated, integrated, unreachable by this load path

## Verdict

**GATED, INTEGRATED, NOT VALIDATED.** No live witness; the point is not counted as closed.

## Transferred

`read_aligned_chunk` opening arithmetic: `aligned_offset`, `offset_from_alignment` (the skip),
`bytes_to_read` (rounded up) and the capped staging `buffer_size`.

## Gate

| check | result |
|---|---|
| 2161 draws over offsets 0..40, sizes 0..40, alignments 1..32, three caps, four fields each | 2161 checks / 0 failures |
| mutations | **7/7 caught** |

Mutations: rounding-up dropped, skip set to the absolute offset, buffer ignoring the cap, mask off by
one, no rounding down, cap compared against `size`, unaligned byte count.

## Post-mutation artifact (caught by the regression sweep)

The mutation loop left `build/bridge-377.o` compiled from the last **mutated** source while the source
itself had been restored, so a later run of the adaptor reported 607 failures against a correct module.
Rebuilding the object from the restored source returns 2161/0. The lesson: a mutation loop must restore
and rebuild **both** the source and the object it compiled, or it leaves a trap for the next run.

## Why no witness

`read_aligned_chunk` is the DirectIO branch. The model is loaded through `mmap` on this host, so the
branch never runs; the single `mmap` line in the log is the loader's own notice, not a call into this
code. Reaching it needs DirectIO enabled, which is not part of the agreed profile.

## Run

| check | result |
|---|---|
| completion before / after restore | `e68df05b…` both |
| `FG377S_FILE_ALIGN` | **0 occurrences** |
| `FG373S` / `FG345W` / `FG241U` | live |
