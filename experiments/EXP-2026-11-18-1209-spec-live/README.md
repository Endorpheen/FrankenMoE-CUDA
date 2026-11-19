# EXP-2026-11-18-1209: the speculative decision, checked on a live server

The host now calls the ported decision on every configuration and compares it with the native value,
applying the native one either way. Four runs, four configurations:

```
ngram-simple               FG_SP_NMAX goose calls=1 disagreements=0
ngram-map-k                FG_SP_NMAX goose calls=1 disagreements=0
ngram-mod                  FG_SP_NMAX goose calls=1 disagreements=0
ngram-simple,ngram-map-k,ngram-mod   calls=1 disagreements=0
```

The last one is the interesting case: three types in one list force the maximum across them, which is
where an ordering or indexing mistake in the port would show.

## What this establishes, and what it does not

Establishes: the door is reached from `common/`, the arrays the host builds match the port's
expectation, and the value agrees with native for every configuration tried - including a mixed list.

Does not establish anything about the other twelve decisions. They are gated in their own units
(compatibility 10 cases, prefix 7, ngram acceptance 14 steps, EAGLE3 8, value choice 8, draft width
10, type names 11 + 7 rejections, cache level 7, static path 10, source order 8, insertion 200
randomised inputs, lookup precedence 2000, hash 503), but none of them is wired to the host yet.

## Why only one call per run

`common_speculative_n_max` is a configuration-time function: it is called when the speculative
configuration is built, not per token. One call per server start is the honest count, and calling it
in a loop to inflate the number would prove nothing.

The next door to wire is the prefix comparison, because it runs **per accepted draft** and will give
thousands of comparisons from a single generation.
