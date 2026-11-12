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

## P6-03: how to obtain a reference graph trace

The request-level node/shape differential needs the same trace on the flint line. This repository's rules make the flint tree the
reference, and a patched reference is no longer a reference, so the hook was added only to our line (`FG_TRACE_GRAPH`, env-gated,
print-and-return, inert by default, Apex still `e68df05b`).

Options for the owner:

1. **Allow a throwaway copy** of the flint tree in a temporary directory with nothing but this hook added, used once to produce the
   reference trace and then deleted. Nothing in the reference tree itself is modified.
2. **Accept the current evidence**: the API-level trace from EXP339 (identical op sequence and shapes on both lines) as the
   differential, and EXP340 as structural evidence that the real-request graph matches the architecture layer for layer.

Without one of these, P6-03's differential clause cannot be closed honestly.

# Сводка ожидающих решений (обновлено 2026-11-12, сессия 8d5ca282)

Ниже — всё, что упирается не в работу, а в решение. Каждый пункт назван, у каждого указано, что произойдёт при любом выборе.

| # | линия | вопрос | варианты |
|---|---|---|---|
| 1 | P6-02, планировщик | Выкаченная `libggml-base` не воспроизводится из исходников; пересборка меняет числовой выход. | (а) принять новый baseline после пересборки; (б) разрешить восстановление исходника выкатки; (в) оставить как есть и не трогать базу. |
| 2 | `make_qkx3` (P2-13) | 50 расхождений класса `−0` против `+0` в возвращаемом минимуме; пережили исправление округления. | (а) считать знаковый ноль допустимым и записать клаузу; (б) продолжить поиск до нуля. |
| 3 | IQ4_XS-энкодер (P2-13) | 126 расхождений; `d` отличается знаком при близкой величине (0.2685 против 0.2701). | (а) консультация старшего помощника; (б) оставить энкодер незакрытым и не заявлять. |
| 4 | P4-03 | Две правки (лишняя проверка кратности блока и текст ошибок) живут в модуле, слинкованном в `libggml-base`. | зависит от решения 1: без перелинковки базы правки не применить. |
| 5 | P6-03 | Для дифференциала нужна трасса графа эталонной линии. | (а) разрешить одноразовую копию flint-дерева только с хуком и удалить её; (б) принять трассу уровня API (EXP339) как дифференциал. |
| 6 | P6-06 | Генератор даёт ровно одну валидную мини-модель; любой мутант падает нативно. | (а) определить границы «kept set» (только архитектура Apex?); (б) признать отсутствие фикстур для прочих архитектур конфигурационной границей. |

Общее во всех шести: работа остановлена на **фактах**, а не на догадках; в каждом случае известно, что именно проверено и чего не хватает.

## P2-16: what does "owned" mean in "zero owned native executable logic"?

The criterion is satisfiable or unsatisfiable depending on the reading, with identical evidence.

Reading "owned = written by this project": the candidate's own C++ is six integration bridges that project native structures into the
modules, so every computation is either Goose-emitted or upstream, and the clause holds.

Reading "owned = shipped in the candidate": the fourteen native SIMD/architecture primitives counted in P2-18 are native executable
logic in the candidate, and the clause cannot hold without porting kernels the compiler does not support.

P1-11 already treated the backends this way -- naming every native kernel and its reason -- which matches the first reading. A one-line
confirmation is enough to close P2-16; without it the item stays open on a definition rather than on work.
