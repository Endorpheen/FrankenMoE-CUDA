# EXP-2026-11-16-758 — 384 MiB of cache is worth more than the policy gain

Base: `3fad7128`. EXP-757 accepted a better SLRU policy (+1.1 hit-rate points). The natural question is
whether that gain lets the cache shrink and free VRAM. It does not.

## The comparison (hit rate and upload are deterministic counters, 61440 plans)

| cache | protected | hit rate | upload | decode @512 |
|---:|---|---:|---:|---:|
| 4608 | 7/8 | 82.7% | 157537 MiB | 39.19 (38.62-40.99) |
| 4992 | 1/2 (old) | 83.7% | 149006 MiB | 39.81 (39.27-42.01) |
| **4992** | **7/8 (new)** | **84.8%** | **139062 MiB** | **40.43 (40.26-40.43)** |

**The smaller cache with the better policy is worse than the larger cache with the old policy** - 82.7% and
39.19 against 83.7% and 39.81. So the policy gain does not buy back 384 MiB, and no VRAM is freed by taking
it.

That matches TRANSFER-BUDGET's conclusion from the other direction: the cache is 28% of the experts and VRAM
is the binding constraint, so cache size dominates any policy tuning.

## What this rules out

- **"Improve the policy, then shrink the cache"** - not available. Cache size wins at this ratio.
- **A policy/size trade at the top of the range** - 4608 is the largest alternative that fits and it is
  strictly worse on both counters and on decode.

## What it confirms

The 7/8 policy is worth taking **only at the current 4992 MiB**, where it is a free gain on top of the
larger cache. That is what EXP-757 shipped.

## Cost

One configuration, three runs.
