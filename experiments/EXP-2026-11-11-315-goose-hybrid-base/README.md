# EXP315: Base hybrid memory policy in Goose

`llama_memory_hybrid` (P5-05): the layer split between the attention and recurrent
caches, the batch split choice, and the prepare order.

## Verdict

**GATED, NOT INTEGRATED** — the policy is exact; the source tree is read-only. No
performance claim.

## Scope transferred

- **Layer filters.** Each cache has its own external filter; without one the model's
  recurrent pattern decides: attention takes the non-recurrent layers, the recurrent
  cache takes the recurrent ones.
- **Batch split.** A batch of all-output tokens splits by sequence; otherwise the
  equal split is used, with the non-sequential form when the attention cache is not
  unified, and with the rollback window (`n_rs_seq + 1`) kept inside one ubatch.
- **Prepare order.** The recurrent cache is prepared first: it can refuse, and the
  attention preparation is the more expensive one, so a refusal is discovered before
  it is paid for. Empty attention heads are a failed prepare.
- **Attention stream count.** One stream when unified, otherwise one per sequence.

## Gate

| check | result |
|---|---|
| filters, 2 000 layer sets with all four presence combinations | 12 000 checks |
| split choice, 1 000 draws | 3 000 checks |
| prepare order and result, 1 000 draws | 4 000 checks |
| attention stream count | 1 000 checks |
| total | 23 582 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| default attention filter follows the recurrent layers | 6 066 |
| default recurrent filter takes the non-recurrent layers | 6 054 |
| an explicit attention filter is ignored | 1 425 |
| all-output batches use the equal split | 493 |
| the sequential flag follows the wrong condition | 507 |
| the rollback window is one token short | 341 |
| the attention cache is prepared first | 980 |
| empty attention heads are accepted | 250 |

All eight are caught.

The rollback-window mutation is the narrowest because it only matters when
`n_rs_seq > 0` *and* the split is the equal one; both conditions are drawn together
explicitly rather than left to chance.

## Files

- `source/host.goose` — the policy plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
