# EXP-2026-11-24-1832 — Expert path measured: 90.7 GB per request, PCIe already saturated

Status: **MEASUREMENT.** Baseline `37ac0f1d` unchanged (prefill 3122.4,
decode 78.225). No code change adopted.

## What was asked

Measure the expert-path share of the acceptance request before touching it:
expert-plane wait and CPU expert time against the ~10.5 s request.

## What was measured

Instrumented counters (`GOOSE_EXP_TRACE=1`) read as deltas between two `/props`
calls around a warm-up and a measured 32768/512 request (`runs/memtr-074321`,
`runs/extr-074102`), plus the CUPTI trace of the same shape that already existed
on disk (`runs/fullprof-000121/prof.sqlite`).

| quantity (two requests) | measured |
| --- | ---: |
| `cuCtxSynchronize` | 2 482 calls, **1.211 s** |
| `cuEventSynchronize` (expert slots) | **0 calls** |
| expert upload host time | 285 228 calls, **0.127 s**, 181.41 GB |
| `cuMemAlloc` / `cuMemFree` | 27 / 12 calls, 0.017 s |
| `cuMemMap` / `cuMemSetAccess` / `cuMemUnmap` | 100 / 100 / 0 calls, 0.007 s |
| prefill + decode wall | 12.35 s + 10.53 s |

Expert cache counters (same block): **95 076 fills, 1 995 360 hits, 91 641
evictions, 181 409 415 168 bytes uploaded**. That is **90.7 GB per request**,
1.91 MB per fill, 95.5 % hit rate.

From the CUPTI trace of the same request shape:

| device activity | measured |
| --- | ---: |
| kernels | 11 583 launches, **3.724 s** |
| memcpy | 83 186 copies, **2.382 s** for **51.67 GB** |
| memcpy throughput | **21.7 GB/s** |
| memset, synchronisation | < 0.03 s |
| CUDA API time on the host | 100 147 calls, **15.153 s** |

## What the numbers mean

1. **The expert path is not host-limited.** All host-side expert work together —
   upload calls, event waits, mapping, allocation — is **0.15 s** of a 10.5 s
   request, and the expert event ring was never waited on (0 calls). The 5.95 s
   "queue wait" from EXP-1828, the async plan staging from EXP-1831 and the
   deferred waits all produce **zero** because the time is not host work.
2. **The transfers are at link speed.** 51.67 GB in 2.382 s is **21.7 GB/s**, which
   is the practical ceiling of this PCIe 4.0 x16 link. The acceptance request moves
   **90.7 GB** of expert planes, so its transfer floor alone is
   **90.7 / 21.7 = 4.2 s** — before any kernel runs.
3. **The budget of the 10.5 s request** is therefore: ~4.2 s of expert traffic at
   link speed, ~3.5-4 s of kernels, and the remainder in the serialisation between
   the two. Perfect overlap of the two phases would give about **7.7 s ≈ 4260
   tok/s**; today's 10.53 s means roughly 2.8 s of the two phases do not overlap.
4. **The traffic itself is set by geometry, not by policy.** The expert set is
   about 20 GB, the cache is 6.29 GiB, and a 32 768-token prompt at ubatch 4096 is
   eight passes over the expert set; a 95.5 % hit rate across that walk cannot hold
   more than the cache allows. Belady headroom was already measured at 2.3 %
   (EXP-1821 era), and raising the cache was measured as worse because the bank
   splits (`expert_cache 1904/1531`).

## Consequence for the target

- The 3500 target needs 1.13 s. The only segment of that size is the **2.8 s of
  non-overlap between expert traffic and kernel work** (4.2 s + 3.6 s against
  10.5 s). Neither host code nor copy submission can move it, and both were
  measured to zero.
- The next experiment therefore has to raise overlap: prefetch the next
  projection's expert planes while the current one computes, so that part of the
  4.2 s of transfers hides behind the ~3.6 s of kernels. That is a scheduling
  change in the expert cache, not a transport change.
- A larger cache (more VRAM) removes traffic outright: at 20 GB resident the
  4.2 s would nearly disappear. That is a hardware question and is out of scope
  here.

## Artefacts

- `runs/memtr-074321` — exp and memory counters, two requests, props before/after.
- `runs/extr-074102` — first counter block with per-thread processor time.
- `runs/defer-073907` — deferred matrix waits measured: 3103.7 and 3098.4 tok/s,
  bit-exact, so that lever is closed too.
- `runs/fullprof-000121/prof.sqlite` — CUPTI trace used for kernels, memcpy
  volume and link throughput.
