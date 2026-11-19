# EXP-2026-11-19-1385: the head is not weak, the step is expensive

## The numbers, from the server's own report

MTP with `--verbose`, context 40960, one 512-token request:

```
draft acceptance = 0.39205 (276 accepted / 704 generated), mean len = 2.17
     acc per pos = (0.630, 0.357, 0.187)
decode 59.80 tok/s
```

Per-position acceptance decays the way a good drafter's does: **63 %** of first drafts are right, then 36 %, then 19 %.
Mean accepted length 2.17 tokens per verification step.

The per-position statistics P6-11 listed as unfinished exist in the compiled copy already, are collected on every step,
and are readable with `--verbose` - what was missing was knowing where to look, not the instrumentation.

## What the numbers rule out, and what they point at

They rule out the cheap explanation. A weak head would show a flat, low curve and a mean length near 1.0; instead the
first draft is accepted two times in three, which is a working head.

So the ~25 % loss is **overhead per step**, not drafting quality. The candidates, in order of plausibility:

1. the draft context runs the **full 41-layer model** - it is created from the target model with the target's params - so
   producing a 3-token draft costs a forward pass comparable to a decode step;
2. verification is a 4-position batch through the target, plus a KV rewind and replay state that the profile's
   `--ctx-checkpoints 32` machinery maintains;
3. `mean len = 2.17` against two forward passes per step is a theoretical ~1.08x before any overhead, so the mechanism
   starts from near parity and loses the rest to (1) and (2).

## The arithmetic that makes this checkable

With mean accepted length L, a step costs one draft forward (D) plus one verification forward (V). Speedup is
`L / (D/V + 1)`. At L = 2.17 and D ~= V, that is `2.17 / 2 = 1.08x` - a best case of +8 %, not the -25 % observed. The
gap between 1.08 and 0.75 is the mechanism's own overhead, and that is where an optimisation has to work.

## Next, and it is a measurement not a guess

Time the two forwards separately for one step (the server already reports `t_draft`-style stage timers) and see how much
of the step is the draft forward. If the draft forward dominates, the fix is the head's own graph - one layer, not 41 -
and that is squarely "reduce the mechanism's own overhead" under rule 35.

## State for P6-11

| item | state |
|---|---|
| per-position acceptance statistics | **collected and readable** (this card) |
| draft acceptance, Apex, MTP | 0.392 overall, 63 % / 36 % / 19 % by position |
| why it is slower | step overhead; the head itself accepts well |
| in-profile MTP | still does not fit at 262144, ~3 GiB extrapolated from a measured 470 MiB at 40960 |
