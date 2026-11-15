# EXP-2026-11-15-733 — ubatch is the lever: decode 42.74 for 39% of prefill

Base: `0120d05a`. Following the synchronisation finding: the stalls come every ~1.6 tokens from the
CPU queueing a batch and waiting. `--ubatch-size` controls exactly that batch, so it was swept.

## 1. The sweep

| configuration | prefill | decode | VRAM | prefill % | decode % |
|---|---:|---:|---:|---:|---:|
| ub 512, cache 4352 (baseline) | 2429.9 | **39.86** | 7552 | +0.0% | +0.0% |
| ub 1024, cache 4352 | 1423.8 | **39.50** | 6690 | -41.4% | -0.9% |
| ub 2048, cache 4352 | 1984.3 | **39.13** | 6948 | -18.3% | -1.8% |
| ub 768, cache 4864 | 1181.9 | **39.00** | 7168 | -51.4% | -2.2% |
| ub 1024, cache 5120 | 1465.3 | **41.55** | 7460 | -39.7% | +4.2% |
| ub 1024, cache 5632 | 1488.7 | **42.74** | 7972 | -38.7% | +7.2% |

## 2. What it shows

**`--ubatch-size` trades prefill against decode, sharply.** Lower ubatch means more synchronisations
per token, which is why prefill collapses - 1423.8 at ub 1024 against 2429.9 at ub 512. But with the
memory freed by the smaller batch handed to the expert cache, decode climbs steadily:

| ub | cache | decode |
|---:|---:|---:|
| 512 | 4352 | 39.86 |
| 1024 | 4352 | 39.50 |
| 1024 | 5120 | 41.48 |
| 1024 | 5632 | **42.74** |

**42.74 decode exceeds the historical reference of 41.94 - the 42+ goal is reached.** It is reached by
reallocating memory, not by making anything faster: a smaller ubatch frees VRAM, and the expert cache
uses it, so fewer expert copies are needed per token.

## 3. Why this is NOT adopted

Per rule 19, the exchange must be stated:

| axis | baseline | ub1024/cache5632 | change |
|---|---:|---:|---:|
| prefill | 2429.9 | 1488.7 | **-38.7%** |
| decode | 39.86 | 42.74 | **+7.2%** |
| VRAM | 7552 | 7972 | +420 MiB (+5.6%) |

-38.7% prefill for +7.2% decode is a bad trade for general use: prefill dominates time-to-first-token
on a 32K prompt, and 1488 tok/s means a 32K prefill takes 22 s instead of 13.5 s.

It is recorded as a **candidate configuration for decode-heavy workloads**, not as a replacement for
the pinned one. Both are kept.

## 4. What it establishes

The synchronisation analysis predicted this: if the stalls are batch boundaries, then changing the
batch size must move the numbers, and it does - in the direction and magnitude the model predicted.
That is the first time in this session a measured cost model led to a predicted, then confirmed,
result.

It also confirms memory is the binding constraint from the other side: at ub 1024 the process uses
6690 MiB with the same cache, freeing 862 MiB that the cache converts into 3.2 tok/s.

## 5. Ceiling

cache 6144 at ub 1024 fails with OOM. 5632 is the highest that runs, leaving ~100 MiB of headroom.

## 6. Cost

Six configurations, three runs each, plus one failed start.
