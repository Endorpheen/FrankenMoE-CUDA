# EXP353: Batch initialisation stats in Goose

`llama_batch_allocr::init`, the output / coupling / sequence-set section (P6-07).

## Verdict

**GATED, NOT INTEGRATED** — the override, the coupling direction and the set map are exact; the
source tree is read-only.

## Scope transferred

- **outputs**: an entry's flag is taken verbatim; when the caller asked for all outputs and some
  entry is unmarked, **every** flag is overridden to one — not only the missing ones.
- **decision order**: the array exists only when some entry carries a non-zero order, and is then
  fully zero-initialised with the non-zero entries copied in.
- **coupling**: a token assigned to several sequences couples them — `s0` is the entry's first
  sequence and every later `s1` is marked coupled to it, with `has_cpl` set. The reverse pair is
  deliberately not tracked.
- **sets**: one mask per entry, a map from mask to entry indices in entry order, unique ids scanned
  in increasing order, and `seq_idx` mapping an id to its position in that list.

## Gate

| check | result |
|---|---|
| 6 000 draws over 1–8 entries, 1–5 sequences, output_all on and off | 175 680 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| the override spares the last entry | 4 584 |
| an all-output request is ignored | 76 338 |
| decision order is emitted when it is empty | 30 416 |
| only the first sequence of a token is seen | 84 022 |
| the reverse coupling is recorded too | 14 535 |
| a repeated mask is reported as new | 34 302 |
| the first unique sequence is skipped | 22 709 |

All seven are caught.

Two candidate mutations were unusable and recorded rather than counted:

- "the override fills only the missing entries" is **equivalent**: the override only fires when a
  zero exists, so filling zeros and filling everything produce the same flags (0 failures measured);
- "unique ids in decreasing order" as first written kept the increment and **looped forever** — a
  hanging mutation is not a detection, and it also left the source mutated when the driver was
  killed mid-run. Replaced with "skip the first unique sequence", which is observable.

## Files

- `source/host.goose` — the three pieces.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
