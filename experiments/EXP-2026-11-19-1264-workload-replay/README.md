# EXP-2026-11-19-1264: the replay guards against the real class

Two guards decide whether a replay may continue: `token()` refuses an index beyond the recorded output, and
`finish()` refuses when not every route record was consumed or when the emitted count differs from the capture.
Both are ported as rule indices and compared with the real class driven to the same state:

```
workload replay: cases=1000 fails=0
```

500 iterations, each exercising both guards: the token index sampled on both sides of the two recorded outputs,
and the finish state built by actually advancing the native route cursor to the sampled value before calling it.

## Why the harness had to drive the native state

`finish()` reads the object's own cursor, so comparing it with a rule index needs the native object in the same
state - not a hypothetical one. The harness loads a fresh workload and advances the cursor with
`replay_topk` the sampled number of times, which is why the comparison is between two real states rather than
between a call and an assumption.

## State of P9-07

| part | evidence |
|---|---|
| file format, reading | 9 crafted files, every failure mode (EXP-1261) |
| file format, writing | byte-exact against `save()` (EXP-1263) |
| metadata agreement | 2000 cases (EXP-1260) |
| replay guards | 1000 cases (this card) |
| capture bookkeeping, metrics sinks | not ported |

The workload class needs only `fstream`/`sstream`, so every remaining part of this family is verifiable the same
way - white-box include, no model, no GPU.
