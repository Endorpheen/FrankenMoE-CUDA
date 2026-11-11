# EXP303: DSV4 compressed-attention plan, overlap layout

The overlap branch of `dsv4_build_comp_plan`, still with no rollback planes
(`overlap == true`, `n_rs_seq == 0`). P5-03, completing EXP302's plan subcase.

## Verdict

**GATED, NOT INTEGRATED** — exact against a transcription in the transferred
subcase; the source tree is read-only. No performance claim.

## Scope transferred

Everything EXP302 transfers, with the read list built in the overlap layout: the
overlap compressor consumes it as two contiguous halves — every completed
block's *previous-window* gather indices, then every block's *current-window*
indices. The two halves are collected separately while walking the tokens and
concatenated once at the end, and a dummy block appends to both halves.

Still not transferred here: the restore/snapshot planes for `n_rs_seq > 0`, and
the debug logging.

## Gate

| check | result |
|---|---|
| 2 093 generated batches (907 skipped where native throws or asserts) | 124 118 checks / 0 failures |

Compared as in EXP302 — every plan array plus counts, `n_kv` and the graph stream
count — with the read list checked in its concatenated order, which is the point
of this experiment: two lists that are each right but joined in the wrong order
would pass a per-block check and fail here.

## Mutations

| mutation | failures |
|---|---:|
| previous window walks backwards | 6 067 |
| current window overwrites the previous half | 44 157 |
| read count keeps only the previous half | 1 238 |
| dummy blocks fill only one half | 193 |
| previous half is never collected | 10 004 |
| previous window starts at the current block | 8 515 |
| write index uses the block offset instead of the block number | 1 672 |
| duplicate token positions take the first token | 898 |

All eight are caught.

One defect of mine, caught by the count first: converting the module from one read
list to two, I replaced the real-block writes and one dummy, but two dummy sites
kept writing into the shared list because their indentation differed and the text
substitution did not match. The gate reported a wrong read count and then wrong
indices — the count is what made the omission obvious, and that is a reason to
compare counts before contents in every plan gate.

## Files

- `source/host.goose` — the plan plus a one-entry dispatcher.
- `tests/fixture.cpp` — native transcription and comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
