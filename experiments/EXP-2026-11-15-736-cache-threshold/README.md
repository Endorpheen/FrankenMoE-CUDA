# EXP-2026-11-15-736 — 5376 MiB is a real threshold, not a limit: 5504 MiB is 1.9 tok/s worse

Base: `6f0ce4cf`. EXP-735 concluded the cache curve does not saturate and that 5376 was simply the most
that fits. That conclusion holds, but the reason is sharper than "out of memory" - there is a cliff.

## 1. The cliff

| cache MiB | decode | VRAM | note |
|---:|---:|---:|---|
| 5120 | 41.67 | 7718 | |
| **5376** | **42.35** | **7972** | optimum |
| 5504 | **38.37** | 8102 | **-1.9 tok/s** |
| 5632 | intermittent OOM | - | |

5504 is not noise: three consecutive runs gave 38.52, 38.37, 38.28 - a 0.24 spread, all far below
5376's 41.97-42.56.

**1.9 tok/s worse while asking for 130 MiB more.** That is the signature of crossing a residency
threshold: at 8102 MiB of VRAM the device no longer has enough free for the KV cache, the compute
buffers or both, so something spills and the larger expert cache stops helping.

## 2. Why this matters beyond the number

It explains an anomaly that had been recorded but not understood: 5632 MiB fails *intermittently* -
one success, two failures across three attempts. A hard ceiling would fail deterministically; a
threshold near the edge fails when a neighbouring allocation happens to land badly. Both
observations are the same cliff seen from two sides.

It also revises the tuning guidance: the safe configuration is not "as much cache as loads", it is
"**5376 MiB, and do not approach the edge**". 5504 loads without error and runs 5% slower, which is
the kind of failure that would be mistaken for measurement noise if only one run had been taken.

## 3. ub 1536 does not help

| configuration | prefill | decode |
|---|---:|---:|
| ub 1536, cache 5376 | 1705.3 | 42.46 |
| **ub 2048, cache 5376** | **2019.3** | **42.35** |

Same decode, 15.5% less prefill. ub 2048 dominates ub 1536 as it dominated ub 1024 - the batch size
gradient is monotone toward 2048 and there is nothing to gain below it.

## 4. Final configuration set

| configuration | prefill | decode | VRAM | when |
|---|---:|---:|---:|---|
| ub 4096 (reference), cache 4352 | 2429.9 | 39.86 | 7552 | long prompt, short generation (reference profile) |
| **ub 2048, cache 5376** | **2019.3** | **42.35** | **7972** | long generation - best balance |
| ub 1024, cache 5632 | 1488.7 | 42.74 | 7972 | maximum decode, prefill sacrificed |

5376 is now recorded as a threshold with evidence on both sides, not as "whatever fits".

## 5. Cost

Two configurations, six runs. No failed starts in this card.
