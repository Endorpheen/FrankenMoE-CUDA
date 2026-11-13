# EXP308: DSV4 cache construction policy in Goose

`llama_kv_cache_dsv4`'s constructor decisions (P5-03): what the five pieces are
made of — a raw K-only cache, CSA and HCA compressed caches, a lightning indexer,
and three compressor states.

## Verdict

**GATED, NOT INTEGRATED** — the policy is exact; the source tree is read-only.
No performance claim. Allocation and tensor creation stay native.

## Scope transferred

- Every hparams copy has `n_layer_nextn` cleared (not represented here: it is a
  single assignment and the gate would only compare a constant against itself).
- The raw cache and the compressed caches stay **per-sequence** even when the
  public mode is unified — `unified_raw = false` and `unified_compressed = false`
  are explicit decisions, not defaults.
- The raw, CSA, HCA and indexer copies are made K-only by setting both MLA head
  widths to the key head width.
- The indexer runs one key head per layer at the indexer head size, on NeoX rope,
  and fixes the SWA head widths as well as the full ones.
- The CSA filter keeps layers whose compress ratio is 4, the HCA filter keeps 128,
  and the raw filter is the external one alone.
- Compressed cache sizes are `PAD(comp_size(kv, ratio), 256)`.
- Compressor states are `(4, 8, 2*head_k)`, `(128, 128, head_k)` and
  `(4, 8, 2*indexer_head)`.

## Gate

| check | result |
|---|---|
| indexer hparams, 1 000 layer counts | 9 000 checks |
| filters, 1 000 layer sets with and without an external filter | 9 000 checks |
| compressed cache sizes, 2 000 draws | 6 000 checks |
| compressor states, unified flags and the K-only tweak | 7 432 checks |
| total | 31 432 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| indexer keeps two key heads | 4 483 |
| indexer SWA value width uses the key head | 994 |
| indexer rope is not NeoX | 1 000 |
| CSA filter takes the HCA ratio | 2 463 |
| CSA cache is sized with the HCA ratio | 1 761 |
| HCA compressor state halves its rows | 500 |
| raw cache becomes unified | 500 |
| K-only leaves the value width alone | 500 |

All eight are caught. The two `false` flags are the ones worth keeping under test:
"unified" is a public setting, and both caches deliberately ignore it, so a later
change that "simplifies" them into following the flag would be caught here rather
than at a recall regression.

## Files

- `source/host.goose` — the policy plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
