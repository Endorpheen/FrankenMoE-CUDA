# EXP-2026-11-19-1387: the eviction explanation is refuted by its own test

## The theory this card started with

The head's experts are touched only during drafts, so they run cold in the shared SLRU and are re-uploaded every step -
making a draft cost 0.86 of a decode, and a quota for the nextn layer the fix.

## The test, and the result

If the draft were upload-bound, shrinking the cache that serves it must hurt the draft **more** than the control. One
draft per step, context 40960, only `--moe-cache-mib` changed (an exploration: the accepted value is 4992):

| cache | control | MTP, one draft | ratio |
|---|---|---|---|
| 4992 MiB | 79.86 | 75.46 | 0.945 |
| 3072 MiB | 62.72 | 58.75 | 0.937 |

The control loses 21 % when the cache shrinks, which confirms that decode itself is upload-bound. The draft's price,
however, barely moves relative to the control - the ratio is *slightly* better at the smaller cache, not worse. **The
eviction theory is refuted by the measurement designed to support it.**

## What that leaves

A decode writes 512 tokens in 6.4 s, one step per token, about 12.5 ms each, and that step runs 41 blocks. A draft adds
**10.8 ms**. One MoE block cannot be 10.8 ms when 41 of them plus attention and sampling are 12.5 - so the draft's cost
is not its own compute either. It is per-step overhead of a kind that does not scale with the cache:

1. the draft context's graph launch and scheduler run, on a batch of one or two tokens;
2. sampling from the head's logits - 248 320 values, a full chain per draft token;
3. state traffic between the two contexts (KV positions, replay).

The same arithmetic applies to the control's own sampling, which is what makes (2) less likely than (1) or (3) - but none
of them is measured yet.

## The one measurement that separates them

The server reports stage timers per request. A run with the draft context instrumented for its forward alone - or the
`t_draft`-style counters if they exist in this tree - would give the draft's forward time directly, and the difference
between that and 10.8 ms is overhead. That is the next step; no further code change until it is known.

## What this card does not claim

No fix, no quota, no improvement. The theory that motivated the work is withdrawn, and the number it was built on
(0.86 of a decode) stands without an explanation.
