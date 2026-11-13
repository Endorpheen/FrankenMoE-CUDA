# EXP333: KV shift input and rotation matrix copy in Goose

`set_input_k_shift` and `set_input_k_rot` / `set_input_v_rot`.

## Verdict

**GATED, NOT INTEGRATED** — the cell journal and the copy size are exact; the source tree
is read-only.

## Scope transferred

- **Shift input**: one entry per cell of every stream, zero for an empty cell and the
  accumulated shift otherwise. The stream stride is that stream's own cell count, so
  streams of different sizes land where native puts them — a shared stride would shift
  every entry of the second stream.
- **Rotation matrix copy**: the precomputed Hadamard square for the requested width, whole
  tensor, `width² × 4` bytes. A width missing from the table and a width that differs from
  the destination are both native asserts; the port reports **-1** instead, since FFI
  cannot abort the host.

## Deferred: set_input_pos_bucket

The bucket arithmetic mixes float and double: `logf` on a double quotient, a float
multiply, then a double division by `log(128/max_exact)`, then `floorf` on a double sum.
Goose has `f32` and no `double`, so an all-f32 port could land on the other side of an
integer boundary. Approximating it and calling it exact would be the wrong trade, so it is
deferred with this diagnostic rather than ported.

## Gate

| check | result |
|---|---|
| shift journal, 2 000 draws, 1–3 streams of differing sizes | 44 000 checks |
| rotation copy, 500 draws including both rejection paths | 2 985 checks |
| total | 46 985 checks / 0 failures |

The first shift gate compared records **by cell index** instead of by journal order, which
read a neighbouring record once the streams had different widths. The journal is a walk
order, not an index-addressed buffer — the native code writes into `data[]`, so the
distinction only appears when the port reports records instead.

## Mutations

| mutation | failures |
|---|---:|
| an empty cell reports its stale shift | 3 368 |
| every stream uses the first stream width | 5 832 |
| the matrix copy forgets the element width | 612 |
| the matrix copy forgets the square | 598 |
| a width missing from the table is copied anyway | 170 |
| a mismatched width is copied anyway | 156 |
| the shift is read without the stream offset | 4 889 |
| the reported count is zero | 2 000 |

All eight are caught. The two rejection mutations needed the gate to stop assuming the
copy always succeeds — the first version never exercised those paths.

## Files

- `source/host.goose` — shift journal and copy plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
