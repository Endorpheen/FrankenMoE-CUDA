# EXP-2026-11-23-1810 — Small-packet ring in prefill: rejected

Status: **REJECTED.** Prefill -2.95 % against the job-pack candidate. The
candidate of record stays `37ac0f1d` (split + asynchronous job packs).

## Idea and mechanism

The instrumented build counted the prefill uploads of the accepted profile:

```
Goose sync HtoD <=4 KiB    calls 150   bytes 0.00 GB host 0.054 s
Goose sync HtoD 4..128 KiB calls 1865  bytes 0.07 GB host 1.387 s
Goose sync HtoD >=128 KiB  calls 565   bytes 1.69 GB host 1.371 s
```

Two 4096-token requests therefore spend 1.39 s of host time in the 4..128 KiB
class, 0.74 ms per call, all of it on the pageable synchronous path. The
candidate extended the pinned ring used for job packs from "above 4096 bytes" to
"everything up to 8 MiB", so those packets would queue instead of blocking.

## Measurement

A = `checkpoints/cand-prefill-jobpack-split` (`37ac0f1d`), B =
`/tmp/opencode/apex-jobs-split/frankenmoe-infer` (`5c596ec3`, the same build plus
the 8 MiB ring). Both with `GOOSE_DECODE_SPLIT=4` and `GOOSE_DECODE_SPLIT_COUNT=1`,
one warm-up and one measured request, alternating A B B A on 9101.

| arm | prefill tok/s | decode tok/s | prompt_ms | client s | exact |
| --- | ---: | ---: | ---: | ---: | --- |
| rg-A1 | 3107.9 | 77.664 | 10543.3 | 17.212 | yes |
| rg-A2 | 3104.9 | 76.999 | 10553.5 | 17.279 | yes |
| rg-B1 | 3013.7 | 78.243 | 10872.8 | 17.493 | yes |
| rg-B2 | 3015.8 | 78.254 | 10865.3 | 17.481 | yes |

Medians: prefill 3106.4 -> 3014.8 (**-2.95 %**), `prompt_ms` 10548 -> 10869,
decode 77.33 -> 78.25 (+1.19 %, inside the spread of the block). All four runs
reproduce the 512 IDs and the text `f8664331bd30…`, peak VRAM 8774 MiB, swap 0.

## Why it loses

These packets are small and consumed almost immediately: the data is read by the
next kernel on the same stream, so the synchronous copy never waited long, while
the ring adds a pinned memcpy and an event record per packet. The job packs are
different: 393 KiB, 240 of them per request, and each one blocks the host for
about 2.7 ms.

The candidate is not kept. `GOOSE_JOBS_ASYNC` still governs the job-pack path,
which is the accepted change.

## Artefacts

`runs/rg-A1-20261123-205533`, `runs/rg-A2-20261123-205809`,
`runs/rg-B1-20261123-205626`, `runs/rg-B2-20261123-205717`;
`runs/diag-syncup-205002` holds the upload histogram above.
