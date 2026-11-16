# EXP-2026-11-15-746 — the defect is prefill chunking, not ubatch

Base: `f5479f79`. EXP-745 ruled out summation reordering. This isolates the trigger, and it is not the
batch size.

## 1. The experiment

Same prompt prefixes, `temperature = 0`, two tokens out, two ubatch values. The only thing that varies
is whether the prefill fits in **one chunk**:

| prompt tokens | chunks at ub 4096 | chunks at ub 2048 | logits identical? |
|---:|---:|---:|---|
| 64 | 1 | 1 | **yes** - `.000301 / -.000013` in both |
| 2048 | 1 | 1 | **yes** - `-.000188 / -.000325` in both |
| 4096 | **1** | **2** | **no** - `\n\n` -1.405 vs ` latency` -0.058 |

At 64 and 2048 tokens both configurations produce **bit-identical logits**. At 4096 tokens - where the
smaller ubatch splits the prefill in two - they diverge completely.

## 2. What that establishes

**The batch size does not change the computation. The number of prefill chunks does.**

Every earlier measurement that blamed `-ub` was observing a proxy: lowering ubatch from 4096 to 2048
turns a one-chunk 32K prefill into a 16-chunk one, and the state carried between chunks is where the
divergence enters.

That is a much sharper target, and it is consistent with the model's shape: 30 of 41 layers are
recurrent and carry state. A prefill split into chunks re-enters that state path once per chunk; a
single-chunk prefill enters it once.

## 3. Why this is a correctness bug and not arithmetic noise

The two logit vectors at 4096 tokens are not close: `\n\n` at -1.405 versus ` latency` at -0.058. The
probabilities implied (0.245 vs 0.943) describe different models, not different rounding.

Given that the same binary produces identical output when the prefill is not split, **the multi-chunk
path has a defect**. Either the recurrent state is not carried correctly between chunks, or the KV/conv
state is written for the wrong span.

## 4. What this means for the speed work

Nothing about it is retracted further - EXP-743 already withdrew the ubatch-based speed claims. This
card names the actual mechanism behind that withdrawal, which is more useful for fixing it:

- the 43.05 decode figure came from a 16-chunk prefill with corrupted state;
- the reference 39.86-40.99 came from an 8-chunk prefill with the same corruption, so **the reference
  profile is also affected** - it just uses fewer chunks.

That is worth stating plainly: if multi-chunk prefill is broken, then the reference-equivalent status of
ub 4096 is only "the configuration whose output matches the recorded hash", not "a correct
implementation". The recorded hash was produced by the reference binary, and our ub-4096 arm matches it
at 32K - so the corruption, whatever it is, produces the same result as the reference. Which means the
reference has it too, or the split at 4096 happens to match the reference's own split.

## 5. The decisive next test

Compare a **single-chunk** prefill against a **multi-chunk** prefill of the same tokens, with ub 4096
fixed and the prompt padded so that one arm splits and the other does not. If they differ, the chunking
path is defective in this build regardless of ubatch. If they agree, then the difference seen at 4096
tokens is specific to the ub-2048 path and the reference is unaffected.

That test needs no new infrastructure - it is the same two configurations with a chosen prompt length.

## 6. Cost

Six server runs, one request each.
