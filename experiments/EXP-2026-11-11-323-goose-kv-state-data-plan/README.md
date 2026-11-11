# EXP323: KV state data plan in Goose

`llama_kv_cache::state_write_data` (P5-08): the header and the transfer records, without
the bytes.

## Verdict

**GATED, NOT INTEGRATED** — the plan is exact; the source tree is read-only. No
performance claim. The tensor transfers themselves stay native.

## Scope transferred

The header carries the V layout flag, the layer count and both rotation sizes. Keys
follow: per layer the type and row size, then one transfer per range at
`first * row_size`.

For a plain V the values repeat that shape with their own row size. For a transposed V
the element size and GQA width replace the row size, and the embedding dimension is
the outer loop: for every embedding row and every range the offset is
`(first + j * kv_size) * element_size`.

## Gate

| check | result |
|---|---|
| 2 000 draws, both V layouts, 0–3 layers, 0–3 ranges | 65 202 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| the V rotation size is not written | 57 183 |
| the layout flag comes second | 4 000 |
| key offsets drop the row size | 3 443 |
| key sizes drop the row size | 4 136 |
| plain V offsets use the key row size | 1 665 |
| transposed V offsets forget the cache stride | 33 469 |
| transposed V sizes drop the element size | 32 809 |
| the GQA width is not written for a transposed V | 37 974 |

All eight are caught.

## Files

- `source/host.goose` — the plan plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
