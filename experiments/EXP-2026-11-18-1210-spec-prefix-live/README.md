# EXP-2026-11-18-1210: the prefix cross-check, its first firing and the defect it exposed

## Which path is live depends on temperature

`slot.use_spec_rejection()` is `temp > 0`. So:

- `temp > 0` -> `common_sampler_sample_and_accept_n_rejection`, which is what the earlier attempt
  patched the wrong function for;
- `temp = 0` -> `common_sampler_sample_and_accept_n`, where the accepted prefix is a token-agreement
  prefix, and where the hook now sits.

With `temp = 0` and `--spec-type ngram-simple` the counter finally fired:

```
FG_SP_PREFIX goose calls=1 disagreements=1
FG_SP_PREFIX_DIFF goose=-1 native=7 draft=20
```

## The disagreement was mine, and it is the interesting part

`goose=-1` is the door refusing, not disagreeing: I asked it to compare `k = draft.size()` tokens
while the target array only holds the tokens actually sampled (seven, plus the one that broke the
loop). `sp_prefix` refuses when `target.len < k`, correctly. Bounding `k` by both lengths is the fix.

## Where this leaves the decision

After the fix the counter did not print on the next run, so the path was not exercised again in that
attempt, and the live agreement of the prefix decision is **still unmeasured**. What is measured is the
gated synthetic comparison (7 cases) and the fact that the hook is reachable and fires when the greedy
route runs.

Recording it this way matters: "the counter fired once and said -1" could be read as a defect in the
port, and it was a defect in my harness. The distinction is only visible because the door's refusal
value (-1) is distinct from a disagreement (0 or 1).
