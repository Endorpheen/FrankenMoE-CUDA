# EXP-2026-11-18-1200: P6-11 ngram cache insertion policy

`common_ngram_cache_update` inserts, for every ngram size from `ngram_min` to `ngram_max`, one
(key, value) pair for every position `i >= max(inp_size - nnew, ngram_size)`: the key is the
`ngram_size` tokens ending just before `i`, the value is `inp[i]`, and a repeated pair increments
its count.

```
cache update: trials=200 fails=0
```

The reference is the native map semantics (`std::map` keyed by the token vector **and** the value),
compared as a set of unique pairs with their counts, over 200 randomised inputs with varied sizes,
`nnew` and value alphabets small enough to force repeats.

## Two defects of mine, both caught by the gate

1. **No key length in the output.** The port wrote keys at a fixed stride and the gate could not
   tell a size-1 key from the first entry of a size-2 key. The port now returns the size of each
   key, which is also the honest representation: the length is part of the key.
2. **Deduplication compared keys of different lengths.** Comparing only the shared prefix merged
   distinct pairs, so the port produced **fewer** unique pairs than the native map in 53 of 200
   cases. Comparing the sizes first fixed it.

The second one is the interesting failure: the counts were plausible, the totals looked sane, and
only a comparison against the real map surfaced that two different ngrams had been folded together.
