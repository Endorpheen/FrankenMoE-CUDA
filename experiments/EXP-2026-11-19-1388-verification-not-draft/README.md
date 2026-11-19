# EXP-2026-11-19-1388: the draft is cheap - it is verification that costs, because the model is sparse

## The measured split

The draft stage is now timed where it happens and reported with the request (one draft per step, context 40960):

```
    eval time = 6989.24 ms / 512 tokens (13.68 ms per token, 73.11 tok/s)
  draft time =  581.29 ms / 291 calls ( 2.00 ms per call, 8.3 % of eval)
   acceptance = 0.75258 (219 / 291), mean len = 1.75
```

So a draft costs **2 ms**, and the step costs 24.0 ms. Verification therefore costs **22 ms** for a two-position batch.
The decomposition in EXP-1386 - a draft at 0.86 of a decode - was a two-point solve that assumed verification was one
decode; it is withdrawn. The expensive part is the verification pass.

## Why verification is expensive, and why that is not a bug

A single-token decode step costs 12.5 ms. Verifying two positions costs 22 ms, which is **1.76x** for 2x the positions -
almost exactly linear, not batched.

That is what a 256-expert MoE with 8 active experts per token does: a decode step reads ~8 experts per layer, and two
positions read up to ~16, so the expert traffic doubles even though the batch is only twice as wide. The GPU work is
amortised; the **memory traffic is not**. With `--cpu-moe` that traffic comes from RAM, which is the bottleneck this
runtime has had all along.

So speculative decoding's central assumption - that verifying k drafts costs about the same as one decode - does not
hold on this model. It holds on dense models, where a wider batch is nearly free.

## What that means for the whole question

The economics become: a step returns `L` tokens and costs `1 + (L + 1) * v` decodes, where `v` is the near-linear cost of
a verified position. With the measured `v ~= 0.88` and `L = 1.75`, the step costs 3.4 decodes-equivalent against 1.75
tokens - a loss, which is what every configuration measured showed.

For speculation to pay off here, `L` would have to exceed `1 + (L+1) * v`, i.e. acceptance would have to be well above
90 % at several positions - far beyond the 75/36/19 % this head produces.

## What this settles

- the draft is not the problem, and no amount of work on its experts would have helped - the quota idea from EXP-1387
  is retired along with the eviction theory it came from;
- the limiting property is the model's sparsity against RAM-resident experts, not the speculative implementation;
- the record stays where it is, and further speed has to come from the expert path itself.

## Where effort should go next

The same measurement points at the real target: if decode is bound by expert traffic from RAM, then anything that
reduces bytes moved per token - a better cache policy for the **trunk**, a wider residency window, a cheaper expert
format - moves the record. That is the trunk's problem, not speculation's, and it is measurable with the same profile.
