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

## The probe was run: it is the array, not the RNG

Hashing the array the selector receives, per round and in both modes:

```
FG_DIST_IN n=3 hash=b9edfde6      (goose)
FG_DIST_IN n=3 hash=...           (native, first differs at the fourth call)
p-hash differing: 85 of 100 rounds
```

The array differs **before** the draw, so the RNG streams are not the cause and neither is the scratch buffer. What
differs is `p` itself.

## Why `p` differs, stated as the one remaining asymmetry

`llama_sampler_softmax_impl` uses `data[0].logit` as the maximum **when `sorted` is set** and scans otherwise. The
doors call `softmax_in_place(lv, p, n, 0)` - always scanning - so the port takes the true maximum while the native
takes the flag's word for it. On a sorted array those agree; on the **post-transform** array, which the transform
turns from descending into ascending, they do not, and the probabilities differ by one rounding step.

So the port computes the correct normalisation of the values it was given, and the native computes one based on a
flag its own transform has invalidated.

## The residual is the RNG state, and the arrays prove it

With the host normalisation in place, the arrays and the probabilities at the failing round are **identical in both
modes**, and the selection still differs:

```
round 3  n=3  ws=1.532418728 (both)   goose idx=2 tok=16   native idx=1 tok=98
```

The printed `p` for that round matches on every entry (the first differing line is the next round), so a draw from
an identical distribution with identical probabilities produced a different index. That is only possible if the RNG
state differs by then - which means some earlier call consumed a **different number of words** in the two modes.

Candidates, in order of likelihood: the selection sampler (the door branch draws two words explicitly, the native
`std::discrete_distribution` draws through `generate_canonical`), and adaptive's internal draw. The probe that
settles it is a per-call-site word counter in both modes, and it is the only measurement of this thread not yet
made.

## The culprit is the adaptive door, and the residual is the draw

Mixing the switches separates the three stages:

```
all doors off                          baseline
typical+XTC doors on, adaptive off     0 rounds differ
adaptive door on, typical+XTC off     21 rounds differ
```

So the pair of preceding doors is exact even inside the triple, and the divergence comes with adaptive's door. At
adaptive's entry the arrays, the `sorted` flags and the monotonicity are identical in both modes, and **the logits
after the transform are identical too** - only `p` differs, by one ULP.

Adding the native's own normalisation on the door path aligned that one round's `p` (`...c9` on both sides) and
**still left 21 rounds differing**, now by a different draw. That change was reverted for the same reason as the
previous one: it shifted the behaviour without moving the metric.

So the statement the measurements support is now: the door's `p` differs by one rounding step because the native
normalisation trusts a `sorted` flag the door cannot see; with that aligned, the residual difference is in the
**draw**, not the distribution - which points at the RNG words consumed at the selection step, the one thing not yet
printed.

## Every pair is exact; only the triple drifts

```
typical+adaptive         0 of 100 rounds differ
xtc+adaptive             0 of 100 rounds differ
typical+xtc+adaptive    21 of 100 rounds differ
```

Each pair is clean, so this is not "adaptive does not tolerate unsorted input" and not "typical's output is
wrong". The difference needs **all three** stages present, which points at something that only two preceding stages
produce - a transformed array whose maximum is no longer at index 0, so the `data[0]`-versus-scan question can
actually bite. With one preceding stage the maximum happens to stay in place and the two paths agree.

That is consistent with the `p`-hash measurement (the array at the selector differs) and inconsistent with the RNG
and the scratch.

## A candidate fix was tried and reverted

The obvious repair for the max question - let the host perform the final normalisation on both paths, in the same
place - changed the measurement by **nothing**:

```
before: 21 of 100 rounds differ
after : 21 of 100 rounds differ
```

So the `p` difference at the selector does **not** come from which side normalises. The change was reverted rather
than kept: a modification that does not move the measurement is not a fix, and leaving it in would make the next
investigation start from a tree with an unexplained edit.

That also weakens the max-choice explanation: the two paths still disagree after the normalisation is unified, so
whatever produces the different `p` sits earlier.

## The decision this needs, which is not mine

Two options, and they cannot both be true:

1. **match the native exactly** - the port must reproduce the flag's effect, including where the flag is stale. That
   keeps token-for-token equivalence with the reference and carries the inconsistency into the Goose runtime;
2. **keep the port correct** - the runtime normalises against the true maximum. Behaviour differs from the native in
   the last bit for these configurations, which is visible only as a different draw in a setting the acceptance
   profile does not use.

The migration rule this project works under asks for behavioural equality proved by gates, which argues for (1);
the rule about not inheriting defects argues for (2). This is a statement about the target, so it is recorded for
Igor rather than decided here.

## The next probe, named

Log the **RNG word values** consumed per round in both modes, plus the `p` array bitwise at the selection step. If
the words match and `p` differs, it is the recompute. If the words differ, the consumption does, and the round where
they first part is the answer. Both are one print in code that already has the flag.

## Why this is recorded and not chased further in this pass

The configuration is not on the acceptance path - the fixed profile enables neither typical, XTC nor adaptive-p -
and three of the last four measurements in this thread were instrument errors. The two real defects found here
(DRY's context order, XTC's ranking before sorting) are fixed, verified, and dormant in the accepted build.
