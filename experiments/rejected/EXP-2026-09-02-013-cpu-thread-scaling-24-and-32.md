# EXP-2026-09-02-013: rejected 24- and 32-thread arms

- Status: `REJECTED` arms of the accepted 12-thread balanced-profile experiment
- Raw summary: `benchmarks/exp013-cpu-thread-scaling.json`

The 24-thread warm request measured `15.943 tok/s` versus `17.718 tok/s` in the immediately preceding 16-thread request, a `-10.02%` regression. Both had zero physical reads, zero major faults, zero swap, unchanged RSS, and identical output hashes.

The initial 32-thread warmup held only `0.69-0.70 tok/s` through 106 generated tokens. It was interrupted to prevent the guardrail from turning into an hour-long benchmark and is excluded from the completed A/B.

Additional SMT workers increased contention without improving GPU utilization. Revisit these counts only with an explicit CPU-affinity hypothesis or after materially changing the CPU expert kernel.
