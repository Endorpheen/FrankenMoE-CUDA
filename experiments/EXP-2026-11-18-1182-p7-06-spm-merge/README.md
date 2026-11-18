# EXP-2026-11-18-1182: P7-06 SPM merge order

The SPM tokenizer's output depends on the **order** in which bigrams are merged, so that order
is the decision worth porting separately. The native loop pops the best bigram by score with
ties on the smaller left index, skips entries that are no longer adjacent, and after a merge
offers two new bigrams: `(prev(left), left)` and `(left, right+1)`.

`src/spm_merge.goose` returns the merge order.

```
spm merge: trials=300 fails=0
```

The reference is deliberately a **different code path**: a real `std::priority_queue` with the
native comparator, pushing only on adjacency and skipping stale entries on pop, exactly as the
native session does. The port instead re-scans the candidate list each step. Those are
equivalent because a bigram's score is fixed when its text is found in the vocabulary, so the
lazy skip cannot reorder anything - but equivalence is a claim, and 300 randomised trials are
what support it rather than the argument alone.

## My stand was wrong again, not the port

The first runs aborted with `array index out of bounds`. The cause was in my harness: a random
case can list up to 28 pairs for eight symbols, while the wrapper declared sixteen-element
slices. Capping the generator at the slice length fixed it. That is the third stand defect of
this kind in this run, and all three were the same failure - a fixed-size buffer meeting a
count the harness itself could exceed.
