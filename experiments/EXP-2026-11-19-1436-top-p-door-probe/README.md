# EXP-2026-11-19-1436: the top_p door is correct, and the previous explanation of the crash is withdrawn

## What was claimed, and what the CPU probe shows

EXP-1435 said the top_p stage corrupted the sampler state: that `softmax_in_place` overwrote the logits with
probabilities and that the hook then wrote them back into `cur_p->data[i].logit`. **That was wrong**, and it was written
without checking the helper:

```goose
fn softmax_in_place(lv: f32[:], p: f32[:], n: i64, sorted: i64) -> f64 {
    ...
    p[i] = e;          // only p is written
```

and on a hand-made array of six elements:

```
kept=5
  [0] id=11 logit=3.000 p=0.459169
  [1] id=15 logit=2.500 p=0.278500
  [2] id=12 logit=2.000 p=0.168919
  [3] id=10 logit=1.000 p=0.062142
  [4] id=13 logit=0.000 p=0.022861
  [5] id=14 logit=-1.000 p=0.008410
sorted=1 ids-aligned=1 p-sum=1.000000 kept=5
```

Sorted descending, ids aligned to their logits, probabilities summing to one, and the count is the prefix whose cumulative
probability reaches 0.99. The door does what its contract says.

## What this means for the crash

The cause is **not identified**, and the withdrawal is the point: the previous commit's explanation was a plausible
mechanism that a single run appeared to confirm, and it does not survive a direct test. The door is back on by default
because it is correct.

What is in place instead:

| guard | effect |
|---|---|
| `FG_SAMPLE_INVALID` in `llama_sampler_sample` | an out-of-range id prints `selected`, `size`, `sorted` and the two array entries |
| `FG_SERVER_INVALID_TOKEN` in `process_token` | the task fails with a message instead of the process aborting |
| draft validation | an invalid draft token is dropped before a batch sees it |
| `repro.sh` | the scenario the acceptance triple cannot reach is preserved |

The next occurrence will print the array state at the moment the id is invalid, which is the evidence that was missing.

## What this costs

One server run, on a 19 586-token text prompt through the chat template, showed the door active again and a different
distribution selection (`sel=1` where the failing run had `sel=0`), which is what changing the sampler path does - not
evidence either way.

## The honest status

The crash is **guarded but not explained**. The guard converts an abort into a diagnostic message plus a failed task, and
the diagnostic is designed to name the source on the next run.
