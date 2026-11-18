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

The first heap-sort attempt negated the shifted scores before a max-heap
extraction, which produced the reverse order and failed every case; it was
reverted. The corrected version builds the max-heap directly on the shifted
scores and extracts to the tail, giving ascending order with lv, id and p
moving together. It passes the same gate unchanged: `cases=1800 mismatches=0`.
Typical-P therefore runs at O(n log n).

## Unsorted Top-P attempt (reverted)

An unsorted top-p was added (softmax without a sort precondition, ascending
heap sort, cumulative read from the tail, kept suffix reversed to descending).
It failed 747 of 900 cases, returning a constant 5-token cut where the native
algorithm cut at 2, which points at the probability array not being aligned with
the sorted order rather than at the cut rule. Reverted, not shipped. The
validated typical-p (1800/1800) remains the state of this card.

## Unsorted Top-P, second attempt (reverted, sharper diagnosis)

Rewriting the filter so the probabilities are recomputed from the sorted logits
(no second array to fall out of alignment) produced the identical failure:
747/900, constant cut at 5, and equal-count cases still mismatching in the kept
ids. Since the probability path was removed as a suspect, the remaining cause is
the order/semantics of `sm_partial_sort` (the module's top-k sort) when it is
used as a full descending sort: its comparator or its placement of results does
not match `std::sort` descending. The next attempt must verify the primitive on
a fixed array before the filter is rebuilt on it.
