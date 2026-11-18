# EXP-2026-11-18-1031 — P6-10: Typical-P on Goose, differential 1800/1800

## Blocker found first

`goose/sampling/sampler-chain-full.goose` as delivered does not compile with
either qualified compiler (`8b7a56bb…` and `.cache/goose/simd7`):
`src/chain.goose:39: error: expected top-level declaration` at `export fn
sm_mt_seed`. The `export` keyword is not accepted in a module, so the delivered
chain cannot be linked to the shipped library in this form. The working copy
here replaces `export fn` with `fn` and adds a gate entry; the algorithms are
untouched.

## Typical-P

`sm_apply_typical` follows `llama_sampler_typical_apply` exactly: sort by logit
descending, softmax, entropy, shifted = |(-log p) - entropy|, order by shifted
ascending, cumulative until `> typical_p` with `i >= min_keep-1`, kept set left in
shifted order. The sort is a selection sort (O(n^2)); a heap sort is required
before shipping for the full vocabulary — recorded, not hidden.

## Differential result

`src/diff.cpp` compares the Goose function with the native algorithm transcribed
with `std::sort`, 200 random distributions x (tp 0.5/0.9/0.99) x (min_keep 0/1/5):

```
typical-p: cases=1800 mismatches=0
```

Sizes and kept token ids match in every case.

## Heap sort attempt (reverted)

A heap sort over the three parallel arrays was installed to remove the O(n^2)
limit; it returned the full candidate count on every case (1800/1800
mismatches), so the change was reverted rather than shipped. The validated
selection-sort version is the current state. Diagnosis to continue from: the
order produced by the max-heap extraction with negated shifted scores does not
reproduce the native ascending order, and the probability array alignment must
be re-checked after the sort before the O(n log n) version is accepted.
