# EXP298: SWA KV cache policy in Goose

`llama_kv_cache_iswa` (P5-03): the pair of a non-SWA cache over the layers that
are not sliding-window and an SWA cache over the layers that are.

## Verdict

**GATED, NOT INTEGRATED** — the policy is exact; the source tree is read-only.
No performance claim.

## Scope transferred

The parts of the iswa cache that decide what the two caches are:

1. **SWA cache size.** `min(base, n_swa * (unified ? n_seq_max : 1) + n_ubatch)`
   padded to 256 with `GGML_PAD`, and replaced by the base size when `swa_full`
   is set.
2. **Layer filters.** An external filter AND the layer's own SWA bit, split:
   `base(il) = filter(il) && !is_swa(il)`, `swa(il) = filter(il) && is_swa(il)`.
3. **Delegation.** `seq_rm` succeeds only when both caches succeed; the other
   sequence operations go to both; `seq_pos_min`/`seq_pos_max` read only the SWA
   cache, because the base cache is a superset.
4. **Memory breakdown.** The two caches' per-buffer-type sizes summed, ordered by
   key like the native `std::map`.

`init_batch` (the split search over ubatches) is not part of this transfer: it
drives `llama_batch_allocr` and the split policy, which belongs with the batch
allocator rather than with the cache pair.

## Gate

The oracle is a transcription of the size rule, the filters and the map merge,
with the delegation results checked directly.

| check | result |
|---|---|
| size rule, 2 000 random parameter sets | 2 000 checks |
| layer filters, 500 layer counts and masks, with and without an external filter | 4 000 checks |
| `seq_rm` truth table | 200 checks |
| memory breakdown, 500 pairs of maps with overlapping keys | 4 212 checks |
| total | 10 412 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| size padding rounds down | 1 592 |
| size ignores the micro-batch | 899 |
| full-size SWA cache is not honoured | 403 |
| unified caches do not scale by the sequence count | 543 |
| base and SWA filters are swapped | 1 862 |
| `seq_rm` succeeds if either cache does | 104 |
| breakdown overwrites instead of adding | 323 |
| breakdown keeps insertion order | 2 296 |

All eight are caught. The order mutation is a deliberate second failure mode for
the merge: a map is ordered, so building the union in arrival order is wrong even
when every value is right.

One gate defect: the breakdown fixture first drew keys with replacement, so a
side's list could hold the same key twice. Real `memory_breakdown` returns a map,
which is already merged, so the fixture was describing an input the native code
cannot produce. Keys are now drawn without replacement per side; only overlaps
*between* the two caches merge.

## Files

- `source/host.goose` — the policy plus a one-entry dispatcher.
- `tests/fixture.cpp` — transcription and random driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
