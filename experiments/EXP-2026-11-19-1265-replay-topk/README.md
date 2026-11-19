# EXP-2026-11-19-1265: the replay route decision, including the cursor it leaves behind

`replay_topk` decides twice: whether the workload still has a record, and whether the batch shape matches it.
The native also advances its cursor **before** the comparison, so a failing call still consumes a record - a
detail a caller cannot recover from, and the reason the port reports the resulting cursor as well as the verdict.

```
replay_topk: cases=1000 fails=0
```

1000 cases over three cursor positions and seven batch shapes (the correct one plus one perturbation of each of
four fields). Both the verdict and the cursor are compared.

## My harness was wrong again, and in a way worth naming

The class does not expose its error text, so the harness classified failures itself - and my first version
hardcoded "route shape mismatch", which meant an **exhausted** workload was reported as a shape mismatch. 356 of
1000 cases failed on that, and every one of them was my classification, not the port.

The fix classifies from the state the harness itself constructed (`cursor >= routes`), so the pass/fail still
comes from the native call and only the which-failure mapping is computed locally.

## What this closes

With this card the workload family's decision surface is covered:

| part | evidence |
|---|---|
| metadata agreement | 2000 cases |
| file reading, all failure modes | 9 crafted files |
| file writing, byte-exact | 4 captured workloads |
| replay guards (`token`, `finish`) | 1000 cases |
| replay route decision and cursor | 1000 cases |

What remains in P9-07 is the capture accumulator (`capture_topk`), which is record construction rather than a
decision, and the metrics sinks, which are telemetry.
