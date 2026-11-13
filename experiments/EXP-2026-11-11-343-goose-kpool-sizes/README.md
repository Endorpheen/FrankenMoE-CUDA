# EXP343: K-pool context sizes in Goose

The three sizing paths of `llama_memory_hybrid_idx_context`.

## Verdict

**GATED, NOT INTEGRATED** — all three paths and their differences are exact; the source tree
is read-only.

## Scope transferred

| path | rule |
|---|---|
| full context | `kpool_build_sizes` fills only `n_pool_real` from the layout; `n_new`, `n_new_g` and the generation stay zero. It exists so `get_n_kpool()` works during graph reservation, before any batch exists. |
| batch context | raises `n_pool_real` to the worst case the cache can hold — `(size / kpool) * n_seq_max` — because the reserved graph must not grow on the first decode; `n_new` takes that value and `n_new_g` is at least one, since the graph always pools one entry. The worst case must fit the padded count in the 32-bit index space. |
| accessors | `get_n_kpool()` pads `n_pool_real`; `get_n_kpool_new()` returns `n_new_g` — the stable bound, never the count actually built this step. |

`kpool_pad` reserves one slot past the requested count: the last padded pool is always unused.

## Gate

| check | result |
|---|---|
| 3 000 + 3 000 draws across the three paths, with 1/8 draws forcing an empty worst case | 28 482 checks / 0 failures |

The forced empty case is why the "bound may be zero" mutation is caught at all: with both the
layout and the cache empty, the bound must still be one, and only an explicit zero draw
reaches that path (317 checks).

## Mutations

| mutation | failures |
|---|---:|
| the full context reports the worst case | 3 000 |
| an untracked context lays out anyway | 1 518 |
| the worst case is not floored by the layout | 4 759 |
| the graph bound may be zero | 317 |
| n_new is not set to the raised count | 2 177 |
| the worst case drops the sequence factor | 6 262 |
| the padded count does not reserve the trailing slot | 108 |
| the new-count accessor returns the real count | 3 000 |

All eight are caught.

## Files

- `source/host.goose` — the sizing rules plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
