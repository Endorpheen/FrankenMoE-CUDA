# EXP-2026-11-24-1912: Retain a CPU worker team across Inkling prefill phases

Start a fixed team once per microbatch. The workers keep their affinity
and receive independent token ranges at each preparation phase. The
request thread joins all participants at every phase boundary before
submitting the associated GPU work. A generation counter orders worker
wakeups. Arithmetic, layer order, recurrent state updates and tensor
layouts are unchanged.

The same executable runs both arms. Use the 512/16 input and RTX 4070
profile of EXP-1911 with context 1024, Q4_0 K/V, batch and ubatch 512,
and a 384 MiB activation arena. Keep the exact fast sampler enabled.
A uses `I2_PARALLEL_PREP=0`. B uses `I2_PARALLEL_PREP=1 I2_PREFILL_TEAM=1`.
Each server gets one cold expert-cache request and one repeat.

| A–B–B–A mean | Sequential | Retained team |
| --- | ---: | ---: |
| Cold prefill, tok/s | 111.36 | 140.10 |
| Warm prefill, tok/s | 130.76 | 177.37 |
| Cold decode, tok/s | 10.82 | 10.69 |
| Warm decode, tok/s | 10.86 | 10.66 |

Prefill gained in both pairs. All 1086 fingerprints per server, generated
IDs and text matched the previously recorded sequential reference.
The expert fills and uploaded bytes were equal. Board peak was 9819 MiB
in all four arms. Process peak RSS grew by approximately 7 MiB; swap and
system swap-out did not grow. Warm decode moved in opposite directions
across the two pairs. The result is an isolated prefill candidate and
does not establish a decode gain or a combined-runtime Apex gate.

The previously tested strategy of launching workers independently for
each phase had disagreed across repeat fingerprints. The retained team
matched the sequential result in both preliminary repeats and both
candidate arms of the scored block. This experiment does not identify
the underlying cause of the earlier mismatch.

The source patches apply to the frozen short candidate's `prefill.h`
and original parallel-preparation `numeric.goose`. Add
`source/prefill-team.h` beside the patched prefill header. Generate
the updated numerical object with KeelWing and the same module-prefixed
initialization symbols. Link it and the updated Inkling backend against
the frozen sampler/attention candidate archive. The scored receipts are
in `results.json`.
