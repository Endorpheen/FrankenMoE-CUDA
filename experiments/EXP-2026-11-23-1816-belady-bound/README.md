# EXP-2026-11-23-1816 — The eviction policy is within 2.3 % of the offline optimum

Status: **MEASUREMENT, axis closed.** Baseline `37ac0f1d` unchanged
(prefill 3122.4, decode 78.225).

## The trace

`GOOSE_MISS_TRACE` in `cuda-driver.c` writes two streams to one file:

- `P <tensor> <plane-bytes> <count> <ids…>` after every projection: the tensor,
  the plane size and every expert the policy was asked for;
- `M <source> <bytes>` from `fg_cuda_expert_stage_upload`: every copy the policy
  decided to make, which is by definition a miss.

(The earlier attempt in EXP-1814 read the slot array from `cache->work` and
produced all-hits; this one reads the decision itself and is exact.)

One 32768-token request with one predicted token:
`runs/miss-235429/miss.txt` — 1079 projections, 99 618 staged copies,
**65.38 GB of misses** (the diagnostic request; the acceptance request reports
94.8 GB over warm and cold phases together in its props).

## The offline optimum

Belady (MIN) over the 150 527 deduplicated accesses, byte capacity taken from the
run itself (`cuda_expert_cache_bytes` = 6287 MiB), each entry costing its own
plane size, evicting the farthest next use:

| | misses | volume |
| --- | ---: | ---: |
| actual policy | — | 65.38 GB |
| Belady optimum | 100 732 | **63.88 GB** |

Headroom: 1.49 GB, **2.3 % of the miss volume**, about 0.07 s per request at the
measured 20 GB/s. A perfect oracle eviction policy would buy well under one
percent of prefill. No policy change can close the ~8 % gap to 3500 tok/s.

## Where that leaves the prefill target

Every reachable axis is now measured, each with a number:

| axis | evidence | verdict |
| --- | --- | --- |
| host transport | four candidates: -2.95 %, +6.2 %, -0.19 %, -0.08 % | closed |
| MMQ switch set | every variant 2507-2565 against 2655 base | closed, accepted set is the best |
| policy knobs | nobalance 2638.4, onetouchoff 2630.1, temp315 2639.7, temp105 crashes | closed |
| eviction policy | within 2.3 % of Belady | closed |
| cache size | profile fixes 6290 MiB; 6510 measured slower, 7500 refused by the card | closed |
| prefetch by prediction | 0 of 96 162 predictable (EXP-1802) | closed |

Prefill stands at **3122.4 tok/s** in the current block conditions
(~3248 in the quiet-machine equivalent). The measured bound of this
configuration — 12 GiB card, 21.7 GB model in host memory with the expert cache,
fixed profile — is the dependency chain of expert transfers, which every
transport, scheduling and policy experiment above has failed to move.

The two remaining levers are outside the configuration: more VRAM for the expert
cache (larger card or smaller context), or a model whose expert set fits the
cache. Both change the accepted profile and are the owner's call, not a code
change.

## Artefacts

`runs/miss-235429/` (trace, props, response), the tracer build
`/tmp/opencode/apex-miss` (source patch kept in the run dir), the Belady script
inline in the experiment log.
