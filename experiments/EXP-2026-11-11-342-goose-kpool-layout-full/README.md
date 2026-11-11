# EXP342: K-pool layout, whole pass in Goose

`llama_memory_hybrid_idx::kpool_layout_update` — the per-sequence resume/rebuild decision and
the pool scan, all sequences at once.

## Verdict

**GATED, NOT INTEGRATED** — the whole pass is exact, including the two "count is not enough"
rules; the source tree is read-only.

## Scope transferred

**Resume**: a clean sequence whose previous layout is non-empty and whose first position is
unchanged only appends cells strictly after the previous last **(position, cell)** pair — ties
on position are real for M-RoPE images, so the cell index breaks them.

**Rebuild**: a stale sequence, an empty previous layout, an empty sequence, a changed first
position, **or** a resulting count that no longer matches the sequence index. The count rule
matters because an edit can regroup a sequence without changing how many cells it has.

A rebuild after a resume attempt re-copies the cell list and re-takes the position minimum —
the length the resume would have produced must not survive.

**Stream bound**: a non-unified cache gives each sequence its own stream and a sequence at or
beyond the stream count is reset with no layout.

**Pool scan**: from the stored scan point — by order, consecutive cells regardless of
position; by position, only at an offset divisible by the pool size with following cells at
consecutive positions.

## Gate

| check | result |
|---|---|
| 4 000 draws over 1–4 sequences, pool sizes 1–4, both modes, four previous-layout shapes | 63 506 checks / 0 failures |

The four previous-layout shapes are what make the rules visible: a true prefix, a prefix with
a moved first position, a shifted copy, and a layout that starts right but holds cells the
sequence no longer has. The first generator used only true prefixes, and three mutations
passed at 0 — a prefix can never exercise the count rule or the tie-break.

## One defect, found on both sides

The resume length survived a rebuild: the port computed `n_final` from the resume, then
decided to rebuild and scanned pools up to the stale length. The transcription had the same
bug, so the gate stayed green until the generator produced a layout with foreign cells. Both
sides were fixed; the transcription is the oracle, so it was fixed first and the port had to
match it.

## Mutations

| mutation | failures |
|---|---:|
| the previous last cell is ignored when resuming | 694 |
| a changed first position does not force a rebuild | 5 367 |
| a stale sequence keeps its layout | 3 205 |
| a count mismatch does not force a rebuild | 10 312 |
| the stream bound is inclusive | 3 274 |
| order mode advances one cell at a time | 15 354 |
| position mode ignores the pool alignment | 3 479 |
| a rebuild keeps the length and minimum from the resume attempt | 37 914 |
| a resumed sequence starts its scan at zero | 6 522 |

All nine are caught.

## Files

- `source/host.goose` — the whole pass plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
