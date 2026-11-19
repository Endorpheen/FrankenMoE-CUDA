# EXP-2026-11-19-1288: a fixed seed reproduces, a different seed does not - through the doors

```
same seed twice:  identical (60 rounds)
different seed:   different tokens (43 rounds)
```

Three claims, each checked rather than assumed:

1. the same seed produces the same token sequence in two separate processes;
2. a different seed produces a different sequence, so the seed is genuinely used and not silently ignored;
3. both hold with the sampler doors **on**, which is the shipping configuration.

The inputs are generated from a fixed seed of their own, so only the sampler's seed varies between the runs. The
chain is the one the fixed profile builds: penalties, top-k, top-p, temp, dist.

## Why the second claim matters as much as the first

A reproducible stream is trivially obtained by ignoring the seed and returning a constant, or by a door that never
draws. Checking that a **different** seed changes the output is what shows the draw actually comes from the seed -
and it is the check that a broken wiring behind the door would fail.

## Scope

This is determinism of the selection path, not of the whole engine: the model's logits depend on kernels whose
accumulation order is a separate question, and the acceptance runs check that end of it with a token hash.
