# EXP-2026-11-17-921: sampler chain in Goose

Status: in progress.
Base: EXP920, f027ef7a.

## Hypothesis fixed before changes

The sampler arithmetic the fixed Apex profile uses can be answered in Goose:
the MT19937 generator, the canonical double draw, and the top-k / top-p /
min-p / temperature / distribution sequence, producing the same token as
`llama_sampler_chain` on the same logits and the same seed.

The chain the profile builds is
`penalties(no-op) -> dry(off) -> top_n_sigma(off) -> top_k 20 -> typical_p(off)
-> top_p 0.95 -> min_p 0 -> xtc(off) -> temp 1 -> dist`, so the decisions are:

- the partial sort of the logits (`std::partial_sort` through libstdc++'s
  heap_select/sort_heap with `logit > logit`), including which of several equal
  logits survives at the cut;
- the float softmax with double accumulation;
- the cumulative top-p cut with `min_keep`;
- the temperature division (and the greedy path when it is zero);
- the draw: `std::uniform_real_distribution<double>` =
  `generate_canonical<double,53>` = `(r1 + r2*2^32)/2^64` over MT19937, consumed
  even when only one candidate remains.

`expf` and `logf` come from libm: they are primitives, and the native code calls
the same functions.

## Fixed contract and gate

Against `libllama` from the accepted Apex bundle (no weights, no GPU, no server):

1. one draw per (logits case, config, seed) pair, a fresh native chain and a
   fresh Goose state for each;
2. a 32-draw sequence on one case, where both sides advance their generator, to
   prove the RNG stream and not just the first value;
3. logits cases: pseudo-random spread, all-equal ties, two equal maxima, a
   one-hot token, a narrow distribution, a wide spread, and the top logit equal
   to the 20th;
4. configs: (k20, p0.95, temp1), (k40, p0.9, temp0.8), (k100, p0.5, temp1.2),
   (k1), and greedy (temp0);
5. seeds: 0, 1, 42, 123456789, 4294967295.

Exact token equality, no tolerance. The Goose library may import libc and libm
(`expf`, `logf`) only.

## Results

The Goose chain matches `llama_sampler_chain` on **207 comparisons with 0
mismatches**: 7 logit sets (pseudo-random spread, all-equal ties, two equal
maxima, one-hot, a narrow 0.01-wide band, a 600-wide spread, and the top logit
repeated across the cut) x 5 configs x 5 seeds, plus a 32-draw stream where both
generators advance together.

MT19937 was verified separately against `std::mt19937`: 700 draws for each of
seeds 0, 42 and 1, no differences.

`0xFFFFFFFF` is `LLAMA_DEFAULT_SEED` and the native sampler replaces it with a
clock/random value, so it cannot be part of a comparison and is excluded.

### Two defects the gate caught

1. `std::__sort_heap` decrements the end BEFORE popping, so the repaired range
   is one element shorter and the popped root stays at the new end. Popping
   first and repairing over the old range left the array in heap order.
2. `std::__adjust_heap` passes the ORIGINAL hole index as `__push_heap`'s
   `__topIndex`, not the root. Using 0 there reorders equal logits inside
   `make_heap`, which changes which tied candidates survive the cut - visible
   only on tie-heavy inputs, which is why the corpus carries them.

Five mutants are rejected: root instead of hole as the push bound, a single
32-bit draw instead of the canonical double, a 0.9-scaled top-p cut, no
temperature division, and always selecting the first candidate.

### Boundaries

The chain covers the sorted path (`top_k > 0`), which is what the fixed profile
uses. The unsorted top-p branch, `typical_p`, `xtc`, `dry`, `mirostat`,
`adaptive-p` and the penalties arithmetic are not ported; the module refuses the
unsorted case rather than approximating it. `expf`/`logf` are libm primitives,
the same functions the native code calls. The shipping sampler is still C++.
