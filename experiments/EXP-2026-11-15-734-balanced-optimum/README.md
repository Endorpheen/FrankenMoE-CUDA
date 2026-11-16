# EXP-2026-11-15-734 — the balanced optimum: 42.35 decode with prefill still above 2000

Base: `8d374a9c`. EXP-733 found that lowering `--ubatch-size` frees VRAM which the expert cache turns
into decode speed, but at ub 1024 it cost 38.7% of prefill. This sweeps the middle and finds a
configuration that keeps most of prefill.

## 1. Result — six runs

| axis | value |
|---|---:|
| prefill median | **2019.3** |
| decode median | **42.35** |
| decode range | 41.97-42.56 |
| VRAM | 7972 MiB |

Configuration: `--ubatch-size 2048`, `--moe-cache-mib 5376`, everything else as the pinned profile.

## 2. Against the pinned baseline, per rule 19

| axis | baseline (ub4096/cache4352) | this (ub2048/cache5376) | change |
|---|---:|---:|---:|
| prefill | 2429.9 | 2019.3 | **-16.9%** |
| decode | 39.86 | 42.35 | **+6.2%** |
| VRAM | 7552 | 7972 | **+420 MiB (+5.6%)** |
| RAM | 21.4 GiB | not sampled | - |

**And against the historical reference**: decode 42.35 exceeds 41.94 by 1.0%, prefill 2019 is 9.4%
below 2228.76.

## 3. Why this is a real compromise and not a win

Honest reading: -16.9% prefill for +6.2% decode is arguable, not obviously right. Whether it is the
right configuration depends on the workload:

- **decode-heavy** (long generation from a short prompt): this wins clearly.
- **32K prompt, 128 tokens out** (the reference profile): prefill dominates - 16.2 s to first token
  instead of 13.5 s, and only 3 s of generation saved. **The baseline is better there.**

So both are kept as named configurations, and neither is declared the winner:

| configuration | when to use |
|---|---|
| ub 4096 (reference), cache 4352 | long prompts, short generations - the reference profile |
| **ub 2048, cache 5376** | long generations, latency-tolerant prefill |
| ub 1024, cache 5632 | maximum decode (42.74), prefill sacrificed |

## 4. The bandwidth of the trade

Plotting decode against prefill across all measured points:

| ub | cache | prefill | decode |
|---:|---:|---:|---:|
| 4096 | 4352 | 2429.9 | 39.86 |
| 2048 | 4352 | 1984.3 | 39.13 |
| 2048 | 5376 | **2019.3** | **42.35** |
| 1024 | 5120 | 1465.3 | 41.55 |
| 1024 | 5632 | 1488.7 | 42.74 |

ub 2048 with the larger cache dominates ub 1024 entirely: **higher prefill and nearly the same
decode**. That is the useful conclusion - 2048 is the right batch size, and the cache size is what
buys decode.

## 5. Ceiling

cache 5632 at ub 2048 OOMs intermittently (two failures, one success) - it sits exactly at the
memory limit. **5376 is the highest reliable value**, confirmed twice. 6144 always fails.

## 6. Cost

Four configurations, 3-6 runs each, three failed starts.
