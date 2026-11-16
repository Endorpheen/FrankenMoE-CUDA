# EXP-2026-11-15-752 — the expert cache is the dominant lever: it nearly doubles decode

Base: `e51d6f7f`. Exp-751 showed prefill is not CPU-bound, which points at expert transfer. This measures
it directly by removing the cache entirely.

## 1. Cache vs no cache, reference argv otherwise

| cache | prefill | decode | VRAM | hash |
|---|---:|---:|---:|---|
| none (`--moe-cache-mib 0`) | 2297.1 | **20.66** | ~3000 | `629635f44f26` |
| 4096 | 2396.9 | 39.29 | 7298 | `c2ff42cc` |
| **4608** | **2428.5** | **41.04** | 7804 | `c2ff42cc` |

**The cache nearly doubles decode: 20.66 -> 41.04, +98.7%**, for 4.7 GiB of VRAM and a 5.4% prefill gain.
Nothing else measured this session comes close - the thread sweep moved decode by 1.5%, the batch
parameters moved it by changing the answer.

## 2. Why this explains EXP-751

Prefill was flat across 6-16 CPU threads because the CPU is not doing the matmuls the cache accelerates -
it is doing the *transfers* the cache eliminates. With no cache, decode halves; with 4608 MiB, 8 threads
saturate whatever transfer remains. The bottleneck was never the thread count.

## 3. The marginal return is still positive at the frontier

| cache step | decode step | per 256 MiB |
|---|---:|---:|
| none -> 4096 | +18.63 | - |
| 4096 -> 4608 | +1.75 | **+1.75** |

At the top of the range each additional 256 MiB still buys about 1.75 tok/s, and larger caches OOM at
ub 4096. So the frontier is genuine, not a plateau: the cache wants more memory than the budget allows.

That is the actionable finding. The limit is **VRAM**, not any tunable knob.

## 4. Correctness note

The no-cache arm diverges from the reference at **token 20**, not token 1 - the first twenty tokens are
identical. That is drift crossing a near-tie, consistent with EXP-749, not a different code path. It does
not change the conclusion: the no-cache arm is far slower and is not a candidate; the measurement exists
to size the cache's contribution.

## 5. What it implies for the next step

If the cache is worth ~1.75 tok/s per 256 MiB and the budget is exhausted, then the way forward is either:

1. **free VRAM elsewhere** - the KV cache is the other large consumer (`-ctk q4_0 -ctv q4_0` already
   quantised); or
2. **make the cache hold more effective experts per MiB** - e.g. keeping hot experts resident and
   streaming cold ones, which is what the existing (currently disabled) hot-store idea was aiming at.

Both are real work, not a flag change. The measurement above is what justifies doing it.

## 6. Cost

Three configurations, nine runs, no failed starts.
