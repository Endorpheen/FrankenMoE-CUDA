# EXP-2026-11-07-144: one post-GLU Apex diagnostic

Status: **PASS_ONE_FIXED_REQUEST_EQUIVALENCE / NOT B1 ACCEPTANCE**.
One explicitly owner-requested Apex32768/128 request,using unchanged workload
argv and identical saved request bytes. Candidate source is checkpoint9c0a698;
no new backend-API changes were included. No repeated series or microbenchmarks.

| Metric | Original C++ (saved) | Previous B1 (saved) | Post-GLU B1 |
|---|---:|---:|---:|
| Prefill tok/s |2867.846|2620.936|2638.151|
| Decode tok/s |27.978|26.738|27.078|
| RSS peak MiB |21311.262|21313.555|21313.887|
| Process VRAM MiB |3348|3348|3348|
| Process swap KiB |0|0|0|
| Text/IDs/top5 versus original |reference|different|all exact|

All128token IDs,the entire generated text and all saved top5logprob records
exactly match the original C++. The sixth token is38778in both; the previous
B1 produced5194there. The maximum shared top5logprob delta is0for the original
comparison. This is evidence for this request,not a general quality audit.

Process counters include normal startup warmup and the sole request:5346Goose
plans/jobs,170378dispatches,42706worker graphs,42560GLU calls and138GET_ROWS calls.
No stateful CPU Goose path or lifetime transitions were witnessed on this
placement; no claim every migrated module executed.

Both controls are historical. New load average starts8.980and ends8.266;
conditions were not controlled quiet periods. Relative to original:prefill
-8.009%,decode-3.215%;relative to previous B1:+0.657%/+1.273%. Record these
without speed acceptance,tuning or causal performance attribution.

The test server exited normally. Production/default runtime and compiler
source were unchanged; no Qwen run. B1 remains incomplete. Continue the common
CPU backend API/registry/buffer ownership work. The separately requested future
GooseMP roadmap idea does not change this sequence.

Raw records,hashes,argv,request,response,memory samples and comparison:
`../docs/goose-migration/apex-results/b1-diagnostic-post-glu-apex-32k128-*`.
Cumulative history:`../GOOSE_MIGRATION_PLAN.md`.
