# EXP325: KV data restore plan in Goose

`llama_kv_cache::state_read_data` (P5-08).

## Verdict

**GATED, NOT INTEGRATED** — the plan, the record order and the rejection codes are
exact; the source tree is read-only. No performance claim. The transfers stay native.

## Scope transferred

Destination indices are batched into contiguous runs — a maximal walk where each index
is the previous one plus one — so a contiguous block reads as a single transfer.

The header is validated in native order (layer count, capacity, V layout, key rotation,
value rotation). Then the layer loop is outermost everywhere: keys per layer, values per
layer, and for a transposed V the embedding row sits between the layer and the runs at
offset `(run.from + j * cells) * element_size`.

Codes: 1 layer count, 2 capacity, 3 V layout, 4 key rotation, 5 value rotation.

## Gate

| check | result |
|---|---|
| 3 000 draws, both V layouts, 0–3 layers, gaps in the destination indices | 50 384 checks / 0 failures |

The oracle reproduces the native loop nesting, which is what the first run got wrong —
the gate had runs outermost and the module had layers outermost. Native is layers
outermost, and the transcription was corrected, not the module.

## Mutations

| mutation | failures |
|---|---:|
| keys are split into single-cell runs | 44 012 |
| plain values are split into single-cell runs | 2 834 |
| key offsets drop the row size | 5 174 |
| plain value offsets use the key row size | 2 906 |
| transposed offsets forget the cache stride | 35 966 |
| transposed sizes drop the element size | 34 127 |
| the layer count is not validated | abort (the run then reads an invalid layout) |
| the V layout is not validated | abort (same) |

All eight are caught; the two validation mutations are caught by the run aborting rather
than by a recorded mismatch, which is a detection too.

## Files

- `source/host.goose` — the plan plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
