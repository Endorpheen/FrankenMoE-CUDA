# EXP331: KV batch initialisation in Goose

`llama_kv_cache::init_batch` — the split policy around the allocator (P5-09).

## Verdict

**GATED, NOT INTEGRATED** — strategy choice, drain and both failure paths are exact; the
source tree is read-only. No performance claim.

## Scope transferred

The allocator itself is an input here: a stream of ubatches, each with a token count and a
unique-sequence count, ending at a zero-token ubatch.

The strategy is chosen once — a single stream splits simply, a single-assignment batch
with several streams splits by sequence, everything else splits equally. Ubaches are
drained until the zero one. Two things can still fail afterwards: the allocator may not
have placed every token (`used < total`) and preparation may come back empty. Both produce
the same failed-to-prepare result, never a partial batch.

## Gate

| check | result |
|---|---|
| 5 000 draws over 1–3 streams, 0–5 ubatches with a 25% chance each of being the terminating zero, and both failure paths | 28 322 checks / 0 failures |

**The first version of this gate did not test the early exit at all.** It generated only
positive token counts, so the zero-token ubatch never appeared and two mutations passed
uncounted: "the drain ignores the zero ubatch" and "the zero ubatch is counted as taken".
The generator now inserts zero at 25%, and both are caught at 5 183 and 1 189. Recording it
because a gate that cannot reach a branch looks exactly like a gate that passes.

## Mutations

| mutation | failures |
|---|---:|
| a single stream splits by sequence | 1 651 |
| a single-assignment batch splits equally | 1 661 |
| the drain ignores the zero ubatch | 5 183 |
| unplaced tokens are accepted | 3 957 |
| a refused preparation is accepted | 1 778 |
| a failed batch reports success | 1 435 |
| the zero ubatch is counted as taken | 1 189 |
| the strategy is never reported | 3 312 |

All eight are caught.

## Files

- `source/host.goose` — the policy plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
