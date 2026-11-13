# EXP-2026-09-09-049 — CUDA Graph opt-out conditions for target verify (offline)

Status: `DONE` (`kind=offline-analysis`, 2026-09-09). Igor's assignment of 2026-09-09 (the next step
after EXP-048): «найти в текущем коде конкретные условия отказа от CUDA Graph для target verify и определить, какие из них действительно выполняются»
("find, in the current code, the specific conditions for dropping CUDA Graph for target verify,
and determine which of them actually hold"). No runs: reading the code of the current runtime tree
(`work/llama.cpp-exp046-iq2s-verify` — the tree of `build/exp046-default-runtime`, CUDA files
identical to the chain) + re-reading the existing EXP-043 trace (read-only). No code changes.

## Build: graphs are enabled and working

- `GGML_CUDA_GRAPHS:BOOL=ON` (runtime CMakeCache) → `USE_CUDA_GRAPH` is defined
  (common.cuh:1229–1231). The debug logs of graph decisions are wrapped in `#ifndef NDEBUG`
  (ggml-cuda.cu:2805/4508/4518) — the Release build (`-O3 -DNDEBUG`) does not contain them:
  there is no runtime visibility of the decisions without a diagnostic build.
- Empirical proof that the machinery works: during model load (7.67–8.33 s per the trace),
  49 BeginCapture→Instantiate→Launch triples were executed — per-split capture of a warmup
  pass (≈49 GPU splits of a decode-like graph = 48 layers + logits). Capture of the split
  structure works on this server configuration (the same flags), including capture-legality
  of all operations.

## Full inventory of opt-out/disable conditions (file:line)

| # | Condition | Location | Holds for verify? |
|---|---|---|---|
| 1 | Build without graphs (`GGML_CUDA_GRAPHS` OFF) | ggml/CMakeLists.txt:209; common.cuh:1229 | NO (ON) |
| 2 | env `GGML_CUDA_DISABLE_GRAPHS` | common.cuh:1259–1262 | NO (the launcher does not set it) |
| 3 | GPU arch < Volta | ggml-cuda.cu:4472–4477 | NO (Ada cc 8.9) |
| 4 | MUL_MAT_ID with a sync fallback inside a CUDA subgraph | ggml-cuda.cu:2785–2816; needs_sync 1893–1922 | NO: batch < 32 keeps MUL_MAT_ID on the CPU (offload gate ggml-cuda.cu:5641, default 32) — this node is absent from verify's GPU splits; bulk-prefill GPU-MMID runs MMQ on Ada — also not present |
| 5 | Warmup automaton: capture only on the second consecutive call of a key with identical properties; after warmup, any change of properties resets it back to direct | ggml-cuda.cu:4504–4523 | YES — a blocking condition |
| 6 | Property identity: node count + memcmp of the entire `ggml_tensor` struct + src data ptrs/ne/nb | ggml-cuda.cu:2843–2859 | partially — see the open item |
| 7 | Key identity = the split's `nodes[0]` pointer; per-key map, eviction of keys unused for ≥10 s | common.cuh:1458–1485 | keys are stable under llama-reuse; on rebuild the static model yields the same struct addresses (`llm_graph_result::reset()` re-initializes the ggml context on the same buffer, llama-graph.cpp:1342–1352) |
| 8 | llama-level reuse (`can_reuse` over all input tensors: n_tokens == the previous ubatch, kq_mask ne[0] == padded n_kv, etc.) | llama-context.cpp:1398; llama-graph.cpp:49–66, 490–503; padding ≥256 — llama-kv-cache.cpp:1250–1264 | KV growth by itself does NOT break reuse (padding 256); actual reuse is 56/171 (EXP-046/047 logs) |
| 9 | The uid comparison speedup does not work for splits (a fresh uid on every sched pass) | ggml-backend.cpp:1538–1541; ggml-cuda.cu:2828–2835 | YES (a full property comparison on every call; not a blocker, but the uid fast path is unreachable) |
| 10 | ExecUpdate failure → destroy + re-instantiate; no permanent disable flag in this fork | ggml-cuda.cu:2864–2889 | — |

## Empirical data from the EXP-043 trace (existing, read-only)

- In total, 51 BeginCapture/Instantiate/Launch triples: **49 at startup** (7.67–8.33 s — load
  warmup), **1 in the warmup-request phase** (125.442 s), **1 right at the W2 boundary**
  (152.9801 s — the final draft/logits segment; exact attribution not established).
  Destructions (ExecDestroy/Destroy, 51) are stretched over 106.9–284 s — 10-second
  evictions + the process destructor.
- The measured EXP-043 request (395 tokens, `n_predict=0` — it contains no decode): 0 graph
  activity inside the W1/W2 windows; all kernels are individual (W2: 4394 `cudaLaunchKernel`
  + 165 `cuLaunchKernel`).
- A consequence for EXP-048 (a correction in its favor): single-shot batches (391, 4) could
  not be captured in principle — each key is computed once, while warmup requires two
  consecutive identical calls. The W2 window could not show graphs regardless of the state
  of verify.
- But: the warmup-request phase (~104 decode tokens, ~50 verify steps) produced only 1
  event → in real decode, graphs are practically never engaged, even though reuse does
  occur in decode256 runs (56/171).

## Open item (not resolvable statically)

1. Shape arithmetic vs reuse: mean len 2.18 and 234 draft passes / 115 steps (≈2.03) ⇒
   the verify batch is ≈ 3 tokens almost always (n_max=2 ⇒ 1+2). A 2↔3 shape change does
   NOT explain the 56/171 reuse: with an almost constant shape, reuse should have occurred
   on almost every step. The real blocker of `can_reuse` has not been identified
   (candidates: KV-slot/mask-bucket state, n_outputs/logits positions, sampler/seq state,
   defrag).
2. The static model predicts: on a reused step (the same graph object, the addresses
   untouched) the second call of every key should get captured (exactly as happened 49
   times at startup). The trace shows that this does not happen in decode (1 event per
   ~50 steps) ⇒ the properties do change between key calls after all. Candidates: drift
   of data addresses during a galloc re-reserve (rebuild alternates with reuse, a plan
   change yields different data pointers → the memcmp changes → a warmup reset); an
   unidentified field in the memcmp.
3. Resolvers (both require approval to run; they require no or only minimal code in the
   working tree): (a) one nsys decode256 run of the current runtime — count
   cudaGraphLaunch per verify step: how many steps are actually on graphs (empirical data
   with no code changes); (b) a diagnostic build with an env counter of graph-compute
   decisions (the direct / warmup-reset / capture / launch branches) +
   `LLAMA_GRAPH_RESULT_DEBUG=1` — the exact cause of the reset.

## Relation to the EXP-048 hypothesis and the limitation

- If graphs do not work in real verify (the warmup-phase trace is in favor) — the lever
  of reducing launch-CPU exists, but the ceiling is limited by the EXP-048 caveat: sums of
  API times ≠ removable latency, launch overlaps with execution. The ≥2% estimate remains
  unconfirmed until the open item is resolved.
- If they work partially — the share is smaller than the EXP-048 estimate.
- Do not remove syncs and D2H (Igor's condition): the weights branch reads router ids on
  the host (this is precisely the per-layer D2H), split inputs require copies/events;
  removing them is only a separate decision after the dependencies are understood.

## Conclusion

1. The specific opt-out conditions are listed with file:line (the table above). The
   combination of №5+№8 reliably holds for verify: the requirement of two consecutive
   identical calls per key combined with the intermittency of reuse / instability of
   properties between rebuild cycles; single-shot batches (W2 of EXP-043) could not be
   captured in principle.
2. Do not hold: build (№1), env (№2), arch (№3), MUL_MAT_ID-sync (№4).
3. Residual uncertainty: why reused steps are not captured and what blocks `can_reuse`
   in 2/3 of steps given the almost constant shape of 3. Closed by one diagnostic run
   (variants (a)/(b)); the choice is Igor's.

## Artifacts

`results/archive/EXP-2026-09-09-049/`: `analysis.sql`, `offline-results.txt` (queries and
output for the EXP-043 trace, sha256 `f86cd32b…`). No new binaries; the code was not
changed.

## Closure of the open item (2026-09-09, EXP-050)

The open item was resolved by the diagnostic run of EXP-050: "reuse 56/171" was a
misreading of the target context's cumulative counter (171−56 = 115 reuses out of 117
steps, i.e. 98%); the blocker was located not in target verify but in the MTP draft
context (alternating ubatch shapes n=3↔n=1 break allow_reuse and CUDA-graph warmup). See
`experiments/EXP-2026-09-09-050-draft-ubatch-shape-pingpong.md`.
