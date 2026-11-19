# EXP-2026-11-19-1246: the recipe matcher joins the live sweep — 89 comparisons, zero disagreements

The matcher (`ar_match`) was the piece the previous card left out, saying it would need a layer-name generator.
It did not: the names can be built from the same table the sweep already holds.

```
FG_ARCH_SWEEP recipes=89 disagreements=0
```

89 comparisons at every model load: nine names, 27 suffix strings, and 53 matcher cases.

## The invariants, which do not need the native matcher

For every row and slot with a suffix:

- `blk.7.<suffix>.weight` must return **that slot** and write **7** into `lay[0]`;
- `blk.7.<suffix>.scale` must return **-1**. The suffix is a prefix of that name, so a matcher without the
  exact `.weight` tail would stream a per-expert scale tensor - the failure the native comment calls
  load-bearing.

For the fused row's empty slot:

- `blk.7.ffn_down_exps.weight` must return **slot 1**, not the empty slot 2.

These are properties of the contract the streamer relies on, so they are worth checking without a native
counterpart in the same process. They would catch a wrong slot index, a missing `.weight` requirement, and a
fused row whose empty slot shadows the real one.

## Coverage of P9-01 now

| part | state |
|---|---|
| recipe names (9) | verified live |
| `ar_exps` per row | verified live |
| suffix strings (27 slots) | verified live |
| `ar_match` positive and negative cases | verified live |
| `LLM_ARCH_NAMES` (163 names) | not ported - belongs to P7-12 |

So P9-01's recipe registry is closed except for what belongs to another item, and that boundary is stated with
the same clarity as the registry itself.
