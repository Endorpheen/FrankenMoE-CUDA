# Owner decisions blocking closure, 2026-11-12

Eight items are blocked on scope decisions rather than on missing work: each names a mechanism this substrate does not
have, or a boundary that the requirement text does not draw. For every one of them the evidence is already collected, so
the decision is what turns evidence into a closed item.

Current counters: **22 DONE, 50 IN_PROGRESS, 80 TODO** of 152 requirements. The two added today are P2-01 (four located
defects fixed, probe clean for every binary/unary/GLU case, F32/F16/BF16 and strides) and P8-04 (an executable API
compatibility suite covering completion, chat and responses in both stream and non-stream form, usage, finish reasons,
errors and cancellation).

## The decisions

| item | what blocks it | decision needed | closes immediately on the decision |
|---|---|---|---|
| **P3-17** pools | "pinned ring" absent; `.reset = NULL` in both device interfaces; no async allocator, the only `cudaLaunchHostFunc` is inside `#if 0` | accept the restatement in `P3-17-RESTATEMENT.md` | yes: live Goose plans, refuse/fallback decisions, balanced counters, shared fixture 51/0 |
| **P1-10** cache | "pinned ring" absent; requirement names `src/llama-expert-*.cpp`, which does not exist in this tree | accept the restatement in `P1-10-RESTATEMENT.md` | yes: scratch/residency/admission/copy decisions are Goose and witnessed (EXP-264/265/271) |
| **P1-09** distribution | `fg140/141_claim` have no call site; the live barrier is `#pragma omp barrier` (`GGML_OPENMP=ON`), custom threadpool compiled out | accept the restatement in `P1-09-RESTATEMENT.md` | yes: FG143/147/149 live, determinism 8/16/32 threads verified |
| **P5-09** maintenance | defrag is dead code (`defrag_thold` unread, `llama_kv_cells::mv` commented out) | accept the defrag retirement | no: shift/cleanup witnesses still need a small-context run (see below) |
| **P8-03** queue | no backpressure mechanism at all: `defer()` is an unbounded `push_back`, no 429, no cap | rule backpressure out of scope; choose whether the SSE keepalive and budget predicates get a module | mostly: queue, slot selection and batch compatibility are already live and A/B-exact |
| **P8-05** utility endpoints | `/embeddings` and `/rerank` answer 501 (flags off, no pooling or reranker head); `apply-template` is vendored Jinja | choose scope: tokenize only, or bring an embedding model and flag, or own Jinja | tokenize part is already Goosed at the decision level |
| **P8-08** diagnostics | `/metrics` and `/slots` answer 501 (flags off); no watchdog subsystem exists | enable `--metrics`/`--slots` in an agreed configuration, or record both as flag-gated serialisation and the watchdog as absent | yes, on the recording |
| **P6-08** context orchestration | six clauses witnessed; counters and error propagation are mechanisms; CUDA-graph capture/replay is ggml-cuda's automaton | confirm counters/errors as primitives and the automaton as native | yes: 91 distinct witnesses in one accepted run |

## One work item that a decision does not unblock

**Shift ownership (P5-09, and P5-07's early-branch/near-capacity).** The module (EXP-327) publishes a journal that needs
access to three private cell structures, so wiring it means new accessors plus journal replay on a core class -- and the
accepted profile never shifts (`-c 262144`, 32K prompt). The unban condition is recorded in `P5-09-INVENTORY.md`: an
agreed small-context configuration, or a statement that shift ownership is required for a future one.

## What is not in this list

The three live increments of P8-03, the P8-04 suite, and the MoE cache integrations (EXP-264/265) are done and witnessed;
they need no decision. The remaining 80 TODO and 50 IN_PROGRESS requirements are ordinary porting or depend on the eight
decisions above; none of them is waiting on a clarification of this kind.
