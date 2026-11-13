# EXP329: KV update in Goose

`llama_kv_cache::update` — stream copies, the K-shift pass, and the two predicates that
guard it (P5-09).

## Verdict

**GATED, NOT INTEGRATED** — order, guards and the return value are exact; the source tree
is read-only. No performance claim.

## Scope transferred

A mirrored cache reports success immediately.

Stream copies run first, per requested pair and per layer: keys always, values only where a
value tensor exists. A pair never copies a stream onto itself, and both ends must be valid
streams — these are preconditions, not outcomes.

The shift pass is guarded by `can_shift` (Step35 and multi-position models cannot shift).
A shift requested where it is impossible is a native hard abort; the port reports **-7**
so the caller cannot miss it, since FFI cannot throw. A shift graph runs only when RoPE is
enabled, at least one layer uses it, allocation succeeded and compute succeeded; otherwise
the returned flag is unchanged. Shift counters are reset on every stream either way.

The return value means "a shift graph actually ran" — a pure stream copy returns false.

## Gate

| check | result |
|---|---|
| 4 000 draws over 1–4 streams, 0–3 layers, 0–2 pairs, all guard combinations | 26 320 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| a mirrored cache does work | 556 |
| values are copied even where none exist | 2 645 |
| only one layer is copied | 3 637 |
| the shift counters are not reset | 1 407 |
| a shift runs without RoPE | 1 304 |
| a shift runs without any layer using RoPE | 1 116 |
| a failed compute still reports an update | 245 |
| Step35 is allowed to shift | 657 |

All eight are caught. The failed-compute case is the narrowest (245) because it needs the
shift to be requested, possible, RoPE-enabled and allocation-clean at once.

## Files

- `source/host.goose` — update plus the two predicates.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
