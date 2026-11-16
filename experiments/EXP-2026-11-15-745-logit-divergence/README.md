# EXP-2026-11-15-745 — the divergence quantified: 1.6e-3 in log space, 1600x more than reordering

Base: `7c5e166b`. EXP-744 proved the logits differ by ubatch. This measures by how much, which decides
between "reordering" and "defect".

## 1. Token 0: same argmax, different probability

| ub | token | logprob | probability |
|---:|---|---:|---:|
| 4096 | `,` | -0.000103 | 0.999897 |
| 2048 | `,` | -0.001674 | 0.998327 |

Both arms pick the same token - so at this position the computation is *close*. But the probability
differs by **1.6e-3 in log space**.

For reference: f32 summation reordering in a dot product of these sizes produces differences around
1e-7. **This is roughly 1600x larger.** So it is not reordering; it is a genuinely different
computation that happens to agree on the argmax here.

## 2. Token 1: the argmax itself diverges

| ub | token 1 | logprob |
|---:|---|---:|
| 4096 | `\n` | -0.779868 |
| 2048 | ` user` | -0.218605 |

Two different tokens, and the reference's choice is *less* likely under both arms - which is only
possible if the distributions themselves differ, not merely their precision.

## 3. What the size of the divergence implies

A 1.6e-3 log-space difference at token 0 and a completely different argmax at token 1 is consistent with:

- **a different set of experts being selected** (the router is 256-way top-8; one different expert
  changes the output materially);
- **a different accumulation pathway** with materially different rounding, e.g. one arm accumulating a
  long sum in a different type or order of magnitude;
- **a genuine bug** in one of the two paths.

It is *not* consistent with "same computation, different summation order", which was the leading
hypothesis after EXP-744. That hypothesis is now ruled out as sufficient.

## 4. Where to look, narrowed by mechanism

The model is 30 recurrent of 41 layers, and the reference profile runs `--cpu-moe`, so experts live in
RAM and are copied on demand. Two mechanisms depend on batch size in exactly this way:

1. **Expert selection and copy.** The router's top-8 is computed per token, but the *copy* granularity
   depends on the ubatch: at a smaller ubatch the cache may evict differently, and if an evicted expert
   is served from a stale or partially-updated buffer the result changes. That would produce exactly
   this signature - mostly-correct with occasional large deviations.
2. **Recurrent state batching.** The GDN layers carry state across tokens; batch size changes how those
   states are shaped and updated. A shape-dependent bug there would corrupt every recurrent layer.

Both are testable without a model run of the kind that needs approval: they are comparisons of two
configurations of the same binary on the same prompt, which is what this card already does.

## 5. Why this matters more than the 43 tok/s

The whole speed progression of the session (39.86 -> 43.05) was obtained by lowering ubatch. **The
progression is now known to be a change of computation, quantified at 1.6e-3 log-space per token.** So
the 43.05 figure is not "the reference answer, faster" - it is a different model output.

That does not make the speed measurements wrong. It makes them measurements of something else, and the
recommendation must reflect that: **ub 4096 is the only reference-equivalent configuration**, and its
speed is 39.86-40.99.

## 6. The procedural lesson, recorded

The hash was stored by `tools/bench-log.py` from the first card. It was never *compared*. Nine
experiments optimised a number whose correctness was one JSON field away, and the field was read only
when a new configuration was being written up for the pin.

**Rule going forward: a benchmark row whose `content_sha256` differs from the reference is a
measurement of a different computation and must be labelled as such in the same row.** That is a change
to how results are reported, not a new experiment.

## 7. Cost

Three server runs, one request each (two at one token, two at two tokens).
