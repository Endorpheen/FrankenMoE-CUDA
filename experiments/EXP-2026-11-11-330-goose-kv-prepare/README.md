# EXP330: KV batch preparation in Goose

`llama_kv_cache::prepare` — the transactional slot reservation before a batch (P5-09).

## Verdict

**GATED, NOT INTEGRATED** — order, rollback and the empty-on-failure rule are exact; the
source tree is read-only. No performance claim.

## Scope transferred

Each ubatch asks for slots first and only then writes: the layout is kept, the touched
cells are copied aside, the ubatch is applied, and the next ubatch sees the mutated cache.

If any slot search comes back empty, everything is undone **in reverse order** and the
result is empty rather than partial. The head of every stream is restored from the copy
taken before that ubatch — the whole per-stream head vector is snapshotted, not only the
streams the ubatch touched, because a later ubatch may have moved the others.

## Gate

| check | result |
|---|---|
| 4 000 draws over 1–5 ubatches, 1–3 streams, 1–4 cells per ubatch | 49 702 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| the undo never runs | 1 033 |
| a failed prepare returns a partial result | 1 825 |
| the head is not restored | 3 025 |
| every ubatch restores the last head snapshot | 1 583 |
| cells are not snapshotted | 23 141 |
| the ubatch is never applied | 26 296 |
| the loop continues past a failed search | 4 921 |
| the undo restores the newest cells first | 2 895 |
| a successful search is not recorded | 37 468 |

All nine are caught. Reverse order is caught by a mutation that restores from the other
end of the offset table (2 895), so the order is verified by the record stream, not only
assumed.

## Files

- `source/host.goose` — the transaction plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
