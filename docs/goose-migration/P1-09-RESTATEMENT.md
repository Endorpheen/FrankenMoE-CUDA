# P1-09 restatement for the substrate that actually runs

Status: live half is Goose and witnessed; two clauses describe code that is either absent or not compiled. One owner
decision needed.

## Clause by clause

| clause | state here | evidence |
|---|---|---|
| partitioning, job queues | Goose, live | FG143 control/executor, FG147 backend, FG149 pool, witnessed in the accepted run (`FG143_CONTROL`, `FG143_EXECUTOR`, `FG149_*`) |
| claims, barrier state | **no call site in this tree** | `fg140_claim`/`fg141_claim` are defined inside the Goose modules and used only by their own fixtures; grep over `ggml/src` and `src` finds none. The live barrier is `ggml_barrier` in `ggml-cpu.c`, which under this build is `#pragma omp barrier` (see next row) |
| barrier primitive | OpenMP runtime, not ours | build has `GGML_OPENMP:BOOL=ON`, so `GGML_USE_OPENMP` is defined and the custom atomics threadpool path (`#ifndef GGML_USE_OPENMP`, `ggml_barrier`'s own `#else` branch, `ggml_compute_state.thrd/pending`) is compiled out. Owning barrier state would mean owning the OpenMP runtime, which is a library, not a ported decision |
| deterministic outputs and termination at 1/2/8/16 threads | verified on the model | batch threads 8/16/32 all produce `e68df05b` (`docs/goose-migration/BIG-PLAN-AUDIT-20261112.md`) |
| physical allocation, OS placement, non-OpenMP implementation | primitives / absent | OpenMP is on, so the non-OpenMP implementation is not part of this binary; allocation and placement are OS/driver calls |

## Proposed restated criterion (owner decision)

> P1-09 (restated): Goose owns the expert work distribution decisions on the executed path -- partitioning, job queues,
> pool and executor lifecycle, backend task queries and dispatch -- with deterministic outputs verified at the
> supported thread counts (8/16/32 on the model). The claim/barrier clause is retired: the claim route has no call site
> in this substrate and the live barrier is the OpenMP runtime's, which is a library primitive rather than a decision to
> port. Physical allocation and OS placement remain primitives.

With this restatement the item closes on the evidence already collected. Without it, two clauses cannot be satisfied by
any amount of porting.

## If the owner prefers the other option

Reintroducing a claim-based coordination route is a design change: it needs its own design, its own concurrency
failure cases and a new determinism regression at every supported thread count. Nothing in the current runtime asks for
it.
