# EXP326: Lazy VMM KV buffer policy in Goose

The lazy virtual-memory KV allocation from `patches/kv-lazy-vmm.patch` (P5-06): the
buffer choice, the commit plan, and the highest-cell rule the three call sites use.

## Verdict

**GATED, NOT INTEGRATED** — the policy is exact; the patch lives in the read-only
production tree. No performance claim. The backend registry lookup and the range commit
remain native primitives.

## Scope transferred

- **Buffer choice**: the lazy buffer is taken only when the cache is offloaded, the
  value cache is *not* transposed, and the backend actually exposes the buffer type. A
  transposed V spreads each cell across the whole tensor, so the committed range would
  have to be everything and the reservation would buy nothing.
- **Commit plan**: per present tensor one `(offset, size)` = its offset inside the
  buffer and one row per requested cell. Nothing is emitted when the cache is not lazy
  or when no cells are requested.
- **Highest cell**: the largest range end, used by state write, state read and the
  ubatch apply path.

## Gate

| check | result |
|---|---|
| buffer choice, 2 000 draws | 4 000 checks |
| commit plans, 3 000 draws over 0–3 layers and 0–2 streams | 11 290 checks |
| highest cell, 2 000 draws | 2 000 checks |
| total | 16 290 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| the lazy buffer is used without offload | 444 |
| the lazy buffer is used for a transposed V too | 476 |
| the lazy buffer is used when the backend has none | 502 |
| commits happen on a plain buffer | 771 |
| a zero request still commits | 4 |
| absent tensors are committed too | 3 814 |
| the highest cell is taken from the range start | 1 589 |
| the highest cell becomes the lowest | 1 589 |

All eight are caught. The zero-request case is the narrowest (4 checks) because it needs
a lazy cache, a present tensor and `n_kv == 0` in the same draw.

## Files

- `source/host.goose` — the policy plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
