# EXP-2026-11-19-1291: two exact components, one inexact composition

```
typical+xtc   (persistent, 100 rounds):        0 rounds differ
adaptive+dist (persistent, 100 rounds):        0 rounds differ
typical+xtc+adaptive+dist (persistent, 100):  21 rounds differ, first at round 4
```

The prefix matches **including the `sorted` flag** (checked separately: ids and logits hash identically, so the label
is not the difference here). adaptive-p's state advances identically in both modes - the trace of its selection,
its EMA and its accept hit is line-for-line the same. Each part reproduces; their composition does not.

## What that constrains it to

Not the arithmetic of either part, and not the array contract: those are covered by the two zero results. What
remains is **shared mutable state between the parts**:

- the two RNG streams the chain owns (adaptive's and the selection sampler's) - if one mode consumes a different
  number of words anywhere, every later round shifts;
- `fg_scratch`, the buffer the door paths share - if a door leaves it in a state the next stage reads differently;
- or a `p` array that is recomputed on one path and reused on the other, disagreeing in the last bit only for some
  inputs.

## The next probe, named

Log the **RNG word values** consumed per round in both modes, plus the `p` array bitwise at the selection step. If
the words match and `p` differs, it is the recompute. If the words differ, the consumption does, and the round where
they first part is the answer. Both are one print in code that already has the flag.

## Why this is recorded and not chased further in this pass

The configuration is not on the acceptance path - the fixed profile enables neither typical, XTC nor adaptive-p -
and three of the last four measurements in this thread were instrument errors. The two real defects found here
(DRY's context order, XTC's ranking before sorting) are fixed, verified, and dormant in the accepted build.
