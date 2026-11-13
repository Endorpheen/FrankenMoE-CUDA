# EXP300: DSA KV cache policy in Goose

`llama_kv_cache_dsa` (P5-03): the MLA cache plus a lightning-indexer cache, and
the two rules that make this pair different from the MSA and iswa pairs.

## Verdict

**GATED, NOT INTEGRATED** — the policy is exact; the source tree is read-only.
No performance claim.

## Scope transferred

1. **Indexer hyperparameters.** One key head per layer (MQA), key head width
   replaced by `indexer_head_size`, and — unlike MSA — rope forced to NeoX.
2. **Stream count.** `n_stream = unified ? 1 : n_seq_max`.
3. **Two independent filters** (`filter_mla`, `filter_lid`).
4. **Delegation.** `seq_rm` is an AND of both caches; the other sequence
   operations go to both; position observers read the MLA cache.
5. **`can_shift`.** `mla.can_shift && lid.can_shift && mla.size == lid.size` — the
   size equality is the part that is easy to drop and that the gate pins.
6. **`state_read` as a transaction.** The MLA part is read first; if the indexer
   read throws, the MLA part is cleared again so a failed restore leaves nothing
   behind, and the exception propagates.

## Gate

| check | result |
|---|---|
| indexer hyperparameters, 1 000 layer counts | 12 000 checks |
| filter split, 500 layer counts | 1 000 checks |
| `seq_rm` truth table | 200 checks |
| position source by value | 200 checks |
| `can_shift`, 500 combinations of shifts and sizes | 500 checks |
| restore transaction, 200 outcomes | 600 checks |
| total | 16 495 checks / 0 failures |

The transaction is checked by outcome: MLA read always happens, the indexer
result follows the injected failure, and the rollback flag is set only when the
indexer read failed.

## Mutations

| mutation | failures |
|---|---:|
| indexer keeps two key heads | 6 513 |
| indexer rope is not NeoX | 1 000 |
| indexer uses the MLA filter | 1 397 |
| `seq_rm` succeeds if either cache does | 105 |
| position observers read the indexer | 200 |
| `can_shift` ignores the size match | 82 |
| failed restore leaves the MLA part behind | 93 |
| failed restore also loses the MLA read | 93 |

All eight are caught.

One harness defect, found by a crash rather than by a mismatch: the transaction
operation writes its three flags to the output buffer, and the fixture did not
pass that buffer for that operation, so the module wrote through a null pointer.
Every operation that writes now receives the buffer explicitly; a gate that can
segfault is not measuring the module.

## Files

- `source/host.goose` — the policy plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
