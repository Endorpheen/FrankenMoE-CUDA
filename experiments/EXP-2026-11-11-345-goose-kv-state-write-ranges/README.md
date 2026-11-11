# EXP345: KV state write range collection in Goose

`llama_kv_cache::state_write` — which cells a written section carries, and in what shape.

## Verdict

**GATED, NOT INTEGRATED** — the selection, the coalescing and all four masking rules are
exact; the source tree is read-only.

## Scope transferred

A cell is written when it is non-empty, holds the requested sequence (any cell for -1), and —
**only for a real sequence** — is not SWA-masked against that sequence's maximum position. A
whole-cache write therefore never applies the SWA filter, which is why a single-sequence write
can be smaller than the whole.

Selected cells coalesce into ranges: a run opens at the first selected cell and closes just
before the first unselected one or at the end of the stream. The section carries the stream
count first, then per stream its cell count; a stream with zero cells is skipped entirely —
no ranges, no meta, no data.

Masking (native shapes): `none` never; `standard` when `p1 - p0 >= n_swa`; `chunked` when p0
falls before `(p1 / n_swa) * n_swa`; `symmetric` when `|p1 - p0| > n_swa / 2` in integers.

## Gate

| check | result |
|---|---|
| 5 000 draws over 1–3 streams, all four masking types, -1 and real sequences | 39 388 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| a mirrored cache writes state too | 382 |
| a partial write of a non-SWA cache proceeds | 1 093 |
| empty cells are written | 9 244 |
| cells of other sequences are written | 9 390 |
| the whole-cache write applies the SWA filter | 1 358 |
| the standard window is exclusive at the boundary | 338 |
| the chunk start uses p0 | 2 231 |
| the symmetric half is doubled | 558 |
| a stream with no cells is not skipped | 4 244 |

All nine are caught. The boundary one (338) is the classic off-by-one: `>=` is inclusive in
native and an exclusive window still passes every other draw.

## Files

- `source/host.goose` — selection, coalescing and masking.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
