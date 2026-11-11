# EXP318: KV view shapes, strides and n_kv padding in Goose

`llama_kv_cache::{get_k, get_v}` shapes and strides, and the `n_kv` padding of
`get_n_kv` (P5-02).

## Verdict

**GATED, NOT INTEGRATED** — the arithmetic is exact; the source tree is read-only.
No performance claim. Tensor views themselves stay native.

## Scope transferred

- **K view**: always `[head_k, n_head_kv, n_kv, n_streams]`, with row strides that
  walk head → heads → cache → streams, and an offset of one stream stride per `s0`.
- **V view**: with a plain cache the shape and strides match K's (but the token
  stride uses the GQA width, which may exceed `head_v * n_head_kv` — the variable-V
  case); with a transposed V the first two dimensions swap and the row stride follows
  the cache size first.
- **`n_kv`**: the highest used position is padded up to `max(n_pad, 256)`, clamped to
  the cache size, and the maximum over streams is taken. The pad is what keeps the
  graph width constant across batches.

Row sizes are `type_size * ne`, which is what `ggml_row_size` returns for the
non-quantised types this cache uses.

## Gate

| check | result |
|---|---|
| K view, 2 000 draws | 16 000 checks |
| V view, both layouts, 2 000 draws | 12 000 checks |
| `n_kv`, 3 000 draws | 3 000 checks |
| total | 29 000 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| K stride between heads is one head wide | 1 736 |
| K view ignores the stream offset | 961 |
| V stride between tokens uses the head width | 971 |
| transposed V first stride drops the head width | 982 |
| transposed V stream stride drops the head count | 990 |
| `n_kv` ignores the 256 floor | 750 |
| `n_kv` does not pad the used position | 1 236 |
| `n_kv` clamps from below | 2 997 |

All eight are caught.

## Files

- `source/host.goose` — the shapes plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
