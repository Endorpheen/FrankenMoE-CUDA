# EXP299: MSA KV cache policy in Goose

`llama_kv_cache_msa` (P5-03): the main cache plus an indexer cache whose
hyperparameters are derived from the main model's.

## Verdict

**GATED, NOT INTEGRATED** — the policy is exact; the source tree is read-only.
No performance claim.

## Scope transferred

The parts that decide what the indexer cache is and how the pair delegates:

1. **Derived hyperparameters.** The indexer runs with one key head per layer
   (`n_head_kv_arr` filled with 1) and the key head width replaced by
   `indexer_head_size`; the rope parameters stay identical to the main cache.
2. **Stream count.** `n_stream = unified ? 1 : n_seq_max`.
3. **Independent filters.** The main cache takes `filter`, the indexer takes
   `filter_idx`; unlike the iswa pair they are not complements of one layer bit.
4. **Delegation.** `seq_rm` is an AND of both caches; the other sequence
   operations go to both; the position observers read the **main** cache. That is
   the contrast with iswa, which reads the SWA cache because there the base is a
   superset — here the indexer is not a superset of anything.

`init_batch` is out for the same reason as in EXP298: the split belongs with
`llama_batch_allocr`.

## Gate

| check | result |
|---|---|
| derived hyperparameters, 1 000 layer counts and indexer head sizes | 12 000 checks |
| filter split, 500 layer counts with independent filters | 1 000 checks |
| `seq_rm` truth table | 200 checks |
| position source, 200 pairs with distinct values | 200 checks |
| total | 14 099 checks / 0 failures |

The position check uses *values*: the gate hands the module a main-cache value and
an indexer value and requires the main one back, so a rule that silently read the
wrong cache would show as a wrong number, not as a constant agreeing with itself.

## Mutations

| mutation | failures |
|---|---:|
| indexer keeps two key heads | 6 345 |
| indexer head size is off by one | 1 000 |
| stream count ignores the unified flag | 487 |
| indexer uses the main filter | 1 326 |
| `seq_rm` succeeds if either cache does | 94 |
| position observers read the indexer | 200 |
| the two filters are swapped | 1 326 |

All seven are caught.

## Files

- `source/host.goose` — the policy plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
