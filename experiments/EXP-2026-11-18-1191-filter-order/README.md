# EXP-2026-11-18-1191: the filter order, measured instead of assumed

The order a filter chain applies is a decision in its own right: the same set of filters in a
different order gives a different token, and every filter can be individually correct while the
result is wrong. So the chain now dumps itself once when `GGML_GOOSE_CHAIN_ORDER` is set, and
the two builds are compared byte for byte.

```
FG_CHAIN_ORDER 0 penalties
FG_CHAIN_ORDER 1 dry
FG_CHAIN_ORDER 2 ?top-n-sigma
FG_CHAIN_ORDER 3 top-k
FG_CHAIN_ORDER 4 ?typical
FG_CHAIN_ORDER 5 top-p
FG_CHAIN_ORDER 6 min-p
FG_CHAIN_ORDER 7 xtc
FG_CHAIN_ORDER 8 temp-ext
FG_CHAIN_ORDER 9 dist
```

```
diff order-on.txt order-off.txt  -> identical, 20 entries
```

The comparison is against the same binary with `GGML_GOOSE_SAMPLER_CHAIN=0`, which is the
stronger form: not two builds that might differ elsewhere, but one build with the decision
switched. The orders are identical.

Two observations worth keeping:

- the `?` prefix marks samplers the server adds optionally (`?top-n-sigma`, `?typical`), so a
  comparison of chain lengths between configurations has to account for them rather than
  expecting a fixed count;
- the order puts `penalties` and `dry` before the truncating filters, which is what makes their
  position matter: applying either after `top-k` would penalise a different candidate set.

The dump is opt-in and prints at most 24 entries per chain, so it cannot affect a measurement
when the variable is unset.
