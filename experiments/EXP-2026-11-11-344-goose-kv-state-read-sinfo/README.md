# EXP344: KV state read orchestration and clearing in Goose

`llama_kv_cache::state_read_sinfo`, `state_clear(seq_id)` and the shared
`state_clear(seq_id, strm, sinfo)`.

## Verdict

**GATED, NOT INTEGRATED** — the guard order, the single up-front clear and both clear
layouts are exact; the source tree is read-only.

## Scope transferred

`state_read_sinfo` is the transaction around the meta and data halves:

- a mirrored cache does nothing; a **partial-only restore of a non-SWA cache also does
  nothing, silently** — that combination has no partial state to read;
- the mirrored layout must have exactly one entry per stream (checked before any read), and
  the section's stream count must match (checked before any write);
- a whole-context read clears the cache **once up front**: clearing per stream inside the loop
  would leave only the last stream cleared;
- a zero-cell stream requires an empty mirrored layout, or the two no longer agree cell for
  cell;
- a failed meta or a throwing data half clears the sequence through the same stream and
  layout **before** throwing, so a half-restored cache never survives.

`state_clear(seq_id)` collects the cells only this sequence holds — a cell another sequence
still uses keeps its data — and delegates. The shared body removes the sequence first, then
zeroes: one contiguous range when the layout allows, per cell otherwise; keys by layer, then
values by layer; a transposed value cache is cleared per embedding row at
`(head + j * cells) * element_size`.

## Gate

| check | result |
|---|---|
| state_read_sinfo, 4 000 draws over every guard | 4 000 + records |
| shared clear, 3 000 draws with contiguous and scattered layouts | 3 000 + records |
| cell collection, 2 000 draws | 2 000 + records |
| total | 42 287 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| a mirrored cache reads state too | 654 |
| a partial-only restore of a non-SWA cache proceeds | 1 063 |
| a mirrored layout with the wrong stream count is accepted | 481 |
| a stream count mismatch is accepted | 889 |
| the whole cache is not cleared up front | 2 173 |
| a zero-cell stream ignores its mirrored layout | 140 |
| a failed restore leaves the cells in place | 676 |
| a single-sequence restore uses the per-stream index | 2 236 |
| the transposed clear forgets the cache stride | 4 595 |

All nine are caught.

## Files

- `source/host.goose` — orchestration and clearing.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
