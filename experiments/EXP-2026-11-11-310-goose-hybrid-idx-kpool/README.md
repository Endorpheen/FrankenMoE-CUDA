# EXP310: Hybrid indexed memory pool layout in Goose

`kpool_pad`, `kpool_rank` and the per-sequence half of `kpool_layout_update`
(P5-05). Pools are fixed by positions relative to the sequence's first one, so a
plain append survives; a stale sequence, or one whose cell count no longer fits,
rebuilds.

## Verdict

**GATED, NOT INTEGRATED** — the layout rules are exact; the source tree is
read-only. No performance claim.

## Scope transferred

- **Pad**: `max(64, PAD(n_pool + 1, 64))`, because the last padded pool is always
  unused.
- **Rank**: `lower_bound` over cells sorted by (position, cell), then an identity
  check — in order mode the rank alone places a token even when positions repeat.
- **Resume vs rebuild**: a clean sequence resumes from the stored scan point when
  the previous layout is non-empty and the first position still matches, keeping
  the pools it already carries; otherwise it rebuilds from zero and drops the scan
  point. A shrinking cell list cannot be resumed.
- **Grouping**: by order, consecutive cells regardless of position; or by position,
  where a pool starts only at an offset divisible by the pool size and the
  following cells must be consecutive positions.

## Gate

| check | result |
|---|---|
| pad, 3 000 draws | 3 000 checks |
| rank, 3 000 draws including duplicate positions | 3 000 checks |
| layout, 4 000 draws over both modes, stale and resumed, with prefix pools | 26 147 checks |
| total | 32 147 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| pad returns one extra block | 3 000 |
| pad rounds one block too far | 38 |
| rank lower_bound uses `<=` | 237 |
| rebuild keeps the old scan point | 5 253 |
| resume accepts a shrinking sequence | 313 |
| position grouping lets a misaligned offset start a pool | 4 260 |
| position grouping only checks the first cell | 2 711 |
| order grouping advances one cell too far | 5 685 |

All eight are caught.

Two candidate mutations were rejected as unobservable rather than missed: the
`max(64, …)` floor is dead code (`PAD(n+1, 64)` is never below 64), and resuming
from an empty layout with a zero scan point produces the same scan as rebuilding.
Both were replaced with observable changes — an extra pad block, and a rebuild that
keeps the old scan point, which is exactly what a stale sequence must not do.

## Files

- `source/host.goose` — the rules plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
