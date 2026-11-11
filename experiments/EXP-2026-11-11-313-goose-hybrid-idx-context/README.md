# EXP313: Hybrid indexed context stepping in Goose

`llama_memory_hybrid_idx_context::{next, apply, kpool_track}` and the stream-count
accessor (P5-05).

## Verdict

**GATED, NOT INTEGRATED** — the stepping rules are exact; the source tree is
read-only. No performance claim.

## Scope transferred

- **`next()`** clears the staleness marks only after a successful ubatch — that is,
  at the first step when a memory exists — then steps the indexer context, advances
  the counter, and steps the base context. Clearing on every step would discard the
  marks the first ubatch exists to consume.
- **`apply()`** ANDs the base and indexer results; only when both succeed *and* the
  pool state is tracked does it extend the layout, rebuild the pool state for this
  ubatch, and pin `i_kpool` to the current step.
- **`kpool_track()`** is derived rather than cached: an indexer memory, a pool size
  above zero, and a non-empty stream-count list.
- **`get_n_stream()`** indexes the stream-count list by the current step and rejects
  a step outside it.

## Gate

| check | result |
|---|---|
| track derivation, 2 000 draws | 2 000 checks |
| step events across all flag combinations, 3 000 draws | 15 000 checks |
| apply events and result, 3 000 draws | 15 000 checks |
| stream accessor including both rejection directions | 1 000 checks |
| total | 19 405 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| track accepts a zero pool size | 101 |
| track accepts an empty stream list | 117 |
| stale marks are cleared on every step | 2 309 |
| the step counter does not advance | 3 000 |
| apply accepts either side succeeding | 2 260 |
| apply does not pin the pool step | 702 |
| apply rebuilds even when a side failed | 1 146 |
| `n_stream` accepts a step past the list | 238 |

All eight are caught.

The two track mutations are the narrowest because each disabled condition needs a
draw that satisfies every other one; both are still reached, which is why the flag
combinations are drawn exhaustively rather than sampled.

## Files

- `source/host.goose` — the rules plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
