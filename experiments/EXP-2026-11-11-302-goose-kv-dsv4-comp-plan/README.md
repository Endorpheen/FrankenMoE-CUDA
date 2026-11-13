# EXP302: DSV4 compressed-attention plan in Goose

`dsv4_build_comp_plan`, the non-overlap subcase with no rollback planes
(`overlap == false`, `n_rs_seq == 0`). P5-03.

## Verdict

**GATED, NOT INTEGRATED** — exact against a transcription in the transferred
subcase; the source tree is read-only. No performance claim.

## Scope transferred

`n_visible` per token (zero for tokens without a position), `state_pos`, `n_kv`
as the running maximum, the state write index/position lists, the read index
list with a three-way source (upcoming token, zero row, persistent row), the CSA
dummy block that keeps the graph shape stable when no block completes, the HCA
dummy write when nothing completes at all, and the persist rows sorted by
destination.

Stated, not implied, as **not transferred here**: the overlap index layout
(previous-window half followed by current-window half), the restore/snapshot
planes for `n_rs_seq > 0`, and the debug logging.

## Gate

| check | result |
|---|---|
| 2 162 generated batches (838 skipped where native throws or asserts) | 87 911 checks / 0 failures |

The oracle is a transcription that keeps the native `std::map` for the token
lookup and the persist rows, compared array by array: `n_visible`, `state_pos`,
write index/position, read index, persist source/destination, `n_kv`, and the
graph stream count.

## Mutations

| mutation | failures |
|---|---:|
| visible blocks divide by the wrong ratio | 4 614 |
| state position uses the wrong modulus | 5 975 |
| write index uses the block offset instead of the block number | 1 613 |
| duplicate token positions take the first token | 970 |
| persist keeps the earlier position | 1 196 |
| dummy write targets the wrong last slot | 152 |
| `n_kv` keeps the last block instead of the maximum | 632 |
| coupled sets still fan out | 953 |

All eight are caught.

Four things the gate caught, in order of cost:

1. **Overlapping output sections.** The read list was sized per token, but a
   dense dummy block writes `ratio` (up to 128) entries, so it ran into the
   persist array and corrupted it. Outputs are now separate pointers — a section
   layout that assumes "a few entries per token" is wrong for this plan.
2. **Duplicate `(sequence, position)`.** The native map keeps the last token; a
   first-match scan gives the wrong source row and fails 970 values.
3. **`n_kv` was untestable at small positions.** `GGML_PAD(n_kv, 256)` floors
   everything below 256, so "maximum" and "last" agreed unless a position above
   1024 was drawn. Positions now reach 5000 in a quarter of the cases.
4. **The HCA dummy's assert.** Native asserts the last cache slot is not live
   before writing there; with wide positions that assert fires, so the
   transcription models it and the fixture skips those draws rather than
   pretending they are valid input.

## Files

- `source/host.goose` — the plan plus a one-entry dispatcher.
- `tests/fixture.cpp` — native transcription and comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
