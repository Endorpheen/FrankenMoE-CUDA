# EXP-2026-11-19-1286: the filter order, tested as the chain the server builds

```
chain order: 120 rounds identical (120 draws, doors: 6 counter lines)
```

## What this covers that the per-door cards cannot

Each door can match on its own and still compose wrongly. A stage that consumes RNG words in a different order,
or leaves the array in a different state, changes what the next stage sees - and every per-door comparison would
still be green.

So this harness builds one `llama_sampler_chain` in the server's order - penalties, top-k, top-p, temp, dist - and
applies it to the same logits for 120 rounds from one seed. The token selected each round must be the same with
every door enabled and with every door disabled.

The counter lines are the witness: three doors print at call 1 and every 100th, so 6 lines across 120 rounds means
top-p, penalties and dist were all reached on every round rather than bypassed.

## Why the selection is the right thing to compare

It is the only observable that depends on the whole composition: sorting, RNG consumption across stages, and the
state each sampler leaves behind. Comparing intermediate arrays would pin the stages but miss a stage that
perturbs the shared RNG.

## Scope

The chain uses the samplers the fixed profile enables. typical, XTC, mirostat and adaptive-p are verified
individually through the same public-API method (EXP-2026-11-19-1282 and -1284); adding them to this chain would
test their ordering too, and is the natural next step rather than a gap in the method.
