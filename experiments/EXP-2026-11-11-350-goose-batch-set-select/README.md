# EXP350: Batch sequence-set selection in Goose

`llama_batch_allocr::split_equal`, the opening block (P6-07).

## Verdict

**GATED, NOT INTEGRATED** — selection, the one-past break and the cursor start are exact; the
source tree is read-only.

## Scope transferred

Before a token is handed out, the split decides which sequence sets take part:

- a row this split already used is skipped;
- a row whose sequence set **overlaps** an already chosen set is skipped — one ubatch never
  carries two overlapping sets;
- in sequential mode only a set whose first sequence id is exactly one past the last accepted id
  is accepted;
- the scan stops once **more than** `n_ubatch` sets are collected, so it can take `n_ubatch + 1`
  before breaking.

An empty selection yields an empty ubatch. Otherwise each set's cursor starts at zero, moves past
rows this split already used and **stops at the first unused row** — not at the end.

## Gate

| check | result |
|---|---|
| 5 000 draws over 1–12 rows, 0–3 ubatch limit, sequential on and off | 45 307 checks / 0 failures |

The set lists are built the way native builds them: one set per distinct sequence-set mask, and
its token list is the rows carrying that mask. The first generator used arbitrary set lists and
walked off the end of the offset table when more sets were chosen than existed — a shape native
cannot produce.

## Mutations

| mutation | failures |
|---|---:|
| used rows are accepted | 11 012 |
| overlapping sets are accepted | 15 431 |
| sequential order is not enforced | 4 812 |
| the break happens at the limit, not one past it | 5 941 |
| the cursor does not move past used rows | 1 484 |
| the cursor skips one row too many | 6 479 |
| the final count is reported as zero | 4 862 |

All seven are caught. One candidate was provably unobservable and replaced with the measurement
recorded: removing the `n_seqs == 0` early exit changes nothing, because the cursor loop then
runs zero times anyway (0 failures).

## Files

- `source/host.goose` — selection and cursor start.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
