# EXP-2026-11-19-1287: DRY holds in composition, typical+XTC does not

```
chain with DRY: identical (DRY counters: 1)
chain stateful: OPEN, 83 rounds differ
```

Two chain configurations, each run with every door enabled and every door disabled, comparing the token selected
each round and the surviving array size.

## Configuration 0 - DRY inside a chain: closed

penalties, DRY, top-k, top-p, temp, dist, with a real accept history that injects breakers (`10`) every fifth
round so DRY has something to read. 100 rounds, identical. This is the composition evidence for the DRY defect
fixed in EXP-2026-11-19-1281: the fix holds when the sampler is fed by the chain rather than by a harness.

## Configuration 1 - typical + XTC + mirostat + adaptive: OPEN

83 of 100 rounds differ. The state the two paths leave after the first two stages is the explanation:

```
goose:  n=51 size=10 sorted_flag=0  [0] id=7 logit=5.181  [3] id=145 logit=5.535
native: n=51 size=7  sorted_flag=1  [0] id=7 logit=5.181  [3] id=77  logit=4.299
```

Two differences, both in the array rather than in a value:

- **the count**: the XTC door keeps 10 where the native path keeps 7, so everything after it - including
  mirostat's truncation width `k` - is computed on a different set;
- **the order**: the goose array is **not monotonic** (entry 3 has a higher logit than entry 0) and is flagged
  unsorted, while the native's stays sorted. The native trims by advancing the base pointer, which preserves
  order; the door compacts in place, which does not.

In isolation both XTC paths keep a sorted array and truncate the same number of times (200 cases, checked
separately). The difference only appears when the array reaching XTC was already modified by `typical` - which is
precisely the composition this card exists to test.

## What is not yet decided

Whether the door should reproduce the native's trimming (advance the base, preserving order) or the caller should
re-sort after the door returns. Both are defensible; choosing between them needs a statement about what a sampler
may do to the array it is given, which is the same open question recorded for `temp_ext` before it turned out to be
my unsorted input. It is **not** on the acceptance path - the fixed profile sets no typical or XTC parameters - so
it is dormant there, and recorded rather than deferred.
