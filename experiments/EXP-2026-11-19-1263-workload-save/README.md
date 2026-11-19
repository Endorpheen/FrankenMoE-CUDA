# EXP-2026-11-19-1263: the workload writer, byte-exact against the real save()

The writer is the other half of the format: `save()` produces what `load()` reads. A port that is merely
"close enough" would break interchange between a captured workload and a replay, and nothing but a byte
comparison would show it.

```
workload save: cases=4 fails=0
```

Four workloads captured by the **real** class (white-box include), saved by it, and written again by the ported
`wl_save` from the same field values; the two files are compared with `memcmp`. The cases cover an empty
workload, negative token ids, multi-digit layer numbers and two routes with nine ids each.

## Three of my own errors, all found by the byte comparison

1. **Recursion is not available in the shape I used.** `rec fn` is rejected here; the integer writer is now a
   digit buffer filled backwards and copied out, which is also cheaper.
2. **`capture_topk` reads `n_tokens * n_expert_used` ids.** I passed one per token, so the native read past the
   vector and wrote garbage into the record - visible in the saved file as `route ... 1 2 3 0 0 0 33 0 4`.
3. **The port's input had to be the expanded ids**, not the original ones, because the record stores one id per
   expert slot.

Each was a harness error; the port was correct once its inputs matched the native's. The byte comparison is what
makes that distinction possible - a "looks reasonable" comparison would have accepted all three.

## What this closes in P9-07

The workload format now round-trips: parse verified against `load()` on nine crafted files (EXP-1261), writer
verified byte-exact against `save()`. What remains in the family is the capture/replay bookkeeping and the metrics
sinks.
