# EXP-2026-11-18-1211: P6-11 synthetic acceptance, the live route for ngram speculation

This is the decision the server actually runs when ngram drafters supply synthetic probabilities, and
it was missing from the port until the accept dispatch was read properly.

```
accept = is_replay || u < prob
if (accept && !is_replay && is_eog) reject
```

```
synth accept: cases=2006 fails=0
```

2000 random cases against the native condition transcribed line by line, plus the boundaries that
matter:

- `u == p` must **not** accept - the comparison is strict, and equality is the only place a
  re-typed `<=` would differ;
- a drafted end-of-sequence with probability 1.0 is still refused, because accepting it would end the
  generation early;
- on replay the same drafted end-of-sequence **is** accepted, because the decision has already been
  made and the token comes from the target.

## Why this belongs to P6-11 and not to P6-10

It sits in the server's speculative acceptance, reads `synth_probs` from the drafter, and consumes the
target's RNG through `common_sampler_sample` - so it is part of the speculative decision set, and it is
the one that is reachable in this environment.

## Still not wired

The function is ported and gated; the host still decides natively. Wiring it follows the pattern that
now works (door resolved from the loaded library, native result applied, disagreement counted), and the
counter would fire thousands of times per generation because this runs per drafted token.
