# EXP346: Recurrent state write and read in Goose

`llama_memory_recurrent::{state_write, state_read}`.

## Verdict

**GATED, NOT INTEGRATED** — both range lists, the ordering of the checks and the read
transaction are exact; the source tree is read-only.

## Scope transferred

A cell is selected when it is non-empty (whole-cache request) or holds the sequence. No SWA
filter and no per-stream split: one count, one meta section, one data section.

The write keeps **two** lists:

- logical ranges over cell indices, coalesced as elsewhere;
- data ranges over *source rows*: `rs_idx[seq] * size + (src >= 0 ? src : i)`. The rollback
  index shifts the row block, and a cell that remembers a previous source writes from there,
  so cells adjacent in cell order but not in row order stay two ranges.

Two checks happen **before anything is written**, even though both depend on the selection:

- a whole-cache write with rollback indices resolves one index per cell from **all** sequences
  in it, and aborts when they disagree;
- a device save refuses more than one logical range.

The section order is count, then meta (logical ranges), then data (row ranges) — not the order
in which the two lists are built.

The read is a transaction: count, meta (the head is meaningful only if meta succeeded), data.
A failure clears the sequence with that head and with the cell count only when meta actually
read one, then throws. A successful read resets the rollback index when the cache uses them.

## Gate

| check | result |
|---|---|
| write, 5 000 draws including both aborts | 40 000 + records |
| read, 2 000 draws over all failure combinations | 4 000 + records |
| total | 44 871 checks / 0 failures |

The first two versions emitted the data ranges while walking, which reordered the section;
the second also ran the device check after emitting the logical ranges, so a refused save
still printed them. Both were caught by the record stream, not by a value.

## Mutations

| mutation | failures |
|---|---:|
| an empty cell is selected for a whole-cache write | 1 714 |
| a cell of another sequence is selected | 13 790 |
| data ranges merge across a gap | 10 749 |
| the previous source of a cell is ignored | 10 647 |
| the rollback index does not shift the row block | 3 301 |
| disagreeing rollback indices in a shared cell pass | 156 |
| device saves accept several ranges | 1 095 |
| a failed read clears with the requested count | 349 |
| a successful read forgets to reset the rollback index | 673 |

All nine are caught. The shared-cell disagreement is the narrowest (156) because it needs a
multi-sequence cell under a whole-cache write with mismatched indices.

## Files

- `source/host.goose` — write and read.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
