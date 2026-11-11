# EXP347: State-write adapter in Goose

The bridge that turns the gated `state_write` decision (EXP345) into something llama can call:
the journal of the module translated back into per-stream cell counts and coalesced ranges.

## Verdict

**ADAPTER GATED, NOT YET CALLED** — this is step 1 of
`docs/goose-migration/integration-readiness-2026-11-11.md`, not part of the accepted Apex path.
It adds no scoped credit on its own.

## What crosses the boundary

Scalars and flat arrays only: the cell positions and sequence masks are widened to `int64` on
this side, the decision runs in Goose, and the journal comes back. The cell struct, the
sequence set and the IO object never leave llama.

## What the adapter had to learn

The module reports a refusal — a mirrored cache, or a partial-only write of a non-SWA cache —
by **writing nothing at all**. Both exits return the same code, so the only signal is an empty
journal. The adapter's first version looked for a return code and therefore turned every refusal
into "zero ranges", which would have written an empty section instead of skipping the write.

## Gate

| check | result |
|---|---|
| 3 000 draws, all four masking types, refusals included | 15 342 checks / 0 failures |
| enabled by default, disabled only by `GGML_GOOSE_KV_STATE_WRITE=0` | 1 check |

| mutation | caught |
|---|---|
| a refusal is reported as success | yes |
| cell counts are not exported | yes |
| ranges are exported with swapped ends | yes |
| the sequence mask is dropped | yes |

## Files

- `source/bridge.c` — adapter plus the generated module it includes.
- `tests/adaptor.cpp` — comparison driver.
- `test_local.py`, `mutate.py` — build/gate and mutation drivers.
