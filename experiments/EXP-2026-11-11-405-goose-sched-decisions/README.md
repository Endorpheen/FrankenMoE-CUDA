# EXP405: scheduler decisions are Goose-owned (APEX.CORE)

## Transferred

| op | rule |
|---|---|
| 0 | the schedule is rebuilt only when something invalidated it |
| 1 | the reservation width is the smaller of the context and the ubatch |
| 2 | the synchronized evaluation is counted as prompt work above one queued token, as a single-token evaluation at exactly one, and not at all when nothing was queued |

## Gate

| check | result |
|---|---|
| 47 checks over reserve flags, widths 0..40 step 8, queue depths −2..5 | 0 failures |
| mutations | **5/5 caught** |

## Run

`FG405S_SCHED_DECISIONS` live with 29 report lines; completion `e68df05b`; restore exact.

## Primitive boundary for APEX.CORE

`ggml_backend_sched_*`, the allocator and the CUDA runtime are the low-level primitives: they allocate,
copy and launch. What Goose owns on the used path is the decision layer — the ten accepted routes
(FG155..FG165), fused-op resolution (FG391S) and these scheduler decisions.
