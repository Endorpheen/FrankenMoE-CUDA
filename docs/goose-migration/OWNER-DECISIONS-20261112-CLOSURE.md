# Owner closure decisions, 2026-11-12

Directive from Igor: close everything that can be closed before the repository
is made public, so the second stage is optimisation on a 100 percent Goose
runtime. This document records the scope decisions that were waiting for the
owner. A decision is not evidence by itself; every row names the evidence that
makes the item close once the decision is taken.

## Accepted restatements

| item | accepted text | evidence that now closes it |
|---|---|---|
| P1-09 | Goose owns expert partition, job queues, pool/executor lifecycle, backend task queries and dispatch; deterministic outputs at 8/16/32 threads. The claim/barrier clause is retired: `fg140/141_claim` have no call site and the live barrier is OpenMP, a library primitive. | FG143/147/149 live and witnessed; determinism 8/16/32 = `e68df05b`; `P1-09-RESTATEMENT.md` |
| P1-10 | Goose owns scratch growth/reuse, bank budget and slot capacity, admission/grouping, upload-run coalescing and its statistics, and the refusal paths. Allocation, buffer and copy calls remain primitives. The pinned-ring clause and `src/llama-expert-*.cpp` are retired: neither exists in this substrate. | EXP-145/150/260/262/263/264/265; `P1-10-RESTATEMENT.md` |
| P3-17 | Goose owns VMM reserve/mapping/actual size/pool extent, pinned host plan with refuse/fallback, free accounting and deregistration, with a balanced lifecycle and zero native fallback. Allocation/mapping/transfer calls are primitives. Device reset, async lifetime tracking and pinned-ring staging are retired: the backend exposes none of them. | `FG195V alloc=9526 free=9526 grow=5 native=0`, `FG196P reg_plan=120 unreg=240 fallback=0`; EXP-260 fixture 51/0; `P3-17-RESTATEMENT.md` |

## Scope decisions

| item | decision | evidence / consequence |
|---|---|---|
| P2-16 | "Owned" means written by this project. Upstream kernels shipped unchanged are not owned; the project's own executable logic is the Goose-emitted modules and the six ABI projection bridges. | P2-18 link map: 33 objects, 13 Goose, 6 bridges, 14 native primitives with reasons; all audit rows assigned and hardware-unavailable variants explicit. |
| P6-03 | Accept the EXP339 API-level node/shape trace plus EXP340 structural trace as the differential. A trace of the reference tree is not produced because modifying the reference would invalidate it. | EXP339 identical op sequence and shapes; EXP340 3819-node request trace, 40 layers, two `ffn_moe_gate` per layer and the hybrid `conv_output_silu` pattern on 30 of 40. |
| P6-08 | Performance counters (clock plus accumulator) and error propagation (throw/abort/return code) are primitives; CUDA-graph capture/replay stays native as ggml-cuda's automaton. | EXP-281 log: 91 distinct Goose witnesses covering construction, warmup, evaluation and output APIs; `P6-08-INVENTORY.md`. |
| P8-03 | Backpressure is out of scope: `defer()` is an unbounded `push_back`, there is no 429 path and no admission cap in this server. The SSE keepalive and generation-budget predicates are below the decision layer and are recorded, not ported. | Queue/slot/batch-compatibility modules live and A/B exact (`FG270Q`, `FG272S`, `FG274B`); `P8-03-REMAINDER.md`. |
| P8-05 | Scope is the registered endpoints of the agreed profile: tokenize, detokenize, apply-template, props and infill. `/embeddings` and `/rerank` are not registered because the flags are off and the model has no pooling or reranker head; their 501 status is the contract. | EXP-282 probe: tokenize `[9419,1814]`, detokenize round trip, template and props byte-identical; `P8-05-INVENTORY.md`. |
| P8-08 | `/metrics` and `/slots` are flag-gated serialisation and are not registered in the agreed profile; the watchdog subsystem does not exist and is retired. `/health` and `/props` are the operational surface. | EXP-282/281: health and props serve; metrics/slots 501 on both lines; no watchdog symbol in the tree; `P8-08-INVENTORY.md`. |

## Explicitly not closed by this document

- **P3-20** still needs the integrity note resolved: the recorded CUDA link names
  `runtime/exp160/build/shared-cuda.o`, which is absent. The definition decision
  above applies, but the missing generated object must be rebuilt or the
  accepted link identified before the item is marked done.
- **P5-07 / P5-09** keep their shift/cleanup/near-capacity clauses. They are a
  wiring-and-run task, not a scope question; the near-capacity half shares the
  P6-02 scheduler blocker.
- **P6-02** remains blocked by the scheduler/CUDA-split defect documented in
  `BLOCKER-20261112-sched-cuda-split.md`; no closure by decision is claimed.
