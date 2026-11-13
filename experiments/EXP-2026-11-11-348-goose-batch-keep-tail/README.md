# EXP348: Batch split tail policy in Goose

`llama_batch_allocr::split_equal`, the `n_keep_tail` block (P6-07, live batching path).

## Verdict

**GATED, NOT INTEGRATED** — the keep/truncate/defer rules and the pool accounting are exact;
the source tree is read-only.

## Scope transferred

The expansion loop already decided how much each sequence set contributes. This block decides
which sets survive, so the trailing tokens of a sequence are never split across ubatches — the
constraint that keeps recurrent rollback snapshots valid.

- **Keep**: the longest prefix of sets where every set is either finished (`remaining == 0`) or
  has at least `n_keep_tail` tokens left.
- **Resolve the first set** when even it violates the rule: extend it to completion if the whole
  set fits in `n_ubatch`, otherwise truncate from the back until exactly `n_keep_tail` tokens
  remain. Truncation **returns** tokens to the pool, so `used` and the counter decrease.
- **Defer**: every set after the kept prefix returns the tokens it had **already taken in this
  ubatch** — its taken prefix, not its whole list.
- **Concatenate** the taken prefixes of the kept sets, in set order.

## Gate

| check | result |
|---|---|
| 5 000 draws over 1–4 sets, tails 0–4, near-capacity ubatches | 83 999 checks / 0 failures |

The gate compares the record stream **and** the final `used` array, which is where the pool
accounting lives. Two layout mistakes were found and fixed on the way: `used` is an `int64`
array (its helpers needed `i * 8`, not `i`), and the per-set list holds the whole set while
`cur_idx` says how much of it was taken — the truncation branch must start from the taken
prefix, not from the list length.

## Mutations

| mutation | failures |
|---|---:|
| the prefix keeps a set right at the tail limit | 16 757 |
| the extend branch is taken when it does not fit | 625 |
| truncation stops one token early | 553 |
| truncated tokens stay marked as used | 54 |
| deferred sets keep their tokens marked | 11 030 |
| the concatenation includes deferred sets | 3 412 |
| a zero tail keeps the block running | 1 970 |

All seven are caught. The narrowest (54) is the pool accounting after truncation: it needs a
truncating draw **and** a token whose `used` bit is observable afterwards.

## Files

- `source/host.goose` — the block plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
