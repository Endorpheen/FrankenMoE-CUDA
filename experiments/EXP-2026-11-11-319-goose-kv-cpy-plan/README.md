# EXP319: KV copy-in plan in Goose

`llama_kv_cache::{cpy_k, cpy_v}` operation sequences and the shape of the V index
tensor from `build_input_v_idxs` (P5-02).

## Verdict

**GATED, NOT INTEGRATED** — the plan is exact; the source tree is read-only. No
performance claim. The tensor operations themselves stay native.

## Scope transferred

- **`cpy_k` and plain-V**: merge the head dimensions of the incoming tensor into one
  row, and when the cache has several streams reshape it to span all of them, because
  the indices are global; then scatter.
- **Transposed-V**: reshape the incoming tensor when its rows are already contiguous
  and copy otherwise; pad when the cache's value width is wider than this token's;
  then flatten both sides to one element per row, because the V indices address
  single head elements.
- **`build_input_v_idxs`**: one index per token, or one per token and maximum value
  element when V is transposed.

## Gate

| check | result |
|---|---|
| `cpy_k` plans, 2 000 draws | 8 000 checks |
| `cpy_v` plans over both layouts, 4 000 draws | 22 000 checks |
| V index shape | 201 checks |
| total | 30 201 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| K copy skips the cross-stream reshape | 2 624 |
| K cross-stream reshape covers one stream only | 1 312 |
| plain-V view keeps one head per row | 1 986 |
| transposed V copies when it could reshape | 1 985 |
| transposed V pads one element early | 1 556 |
| transposed V pads the whole cache width | 1 596 |
| V indices count one per token | 1 722 |
| K view drops the token row | 2 000 |

All eight are caught.

## Files

- `source/host.goose` — the plan plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
