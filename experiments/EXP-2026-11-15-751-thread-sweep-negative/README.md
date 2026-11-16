# EXP-2026-11-15-751 — thread count is already optimal; prefill is GPU-bound

Base: `e1d97c3f`. After cache 4608 was accepted, the next legal lever at the reference batch shape is the
CPU thread count. 32 cores are available and the reference argv uses `-t 8`. Rejected, with the numbers.

## 1. Results (cache 4608, reference argv otherwise, three repeats each)

| `-t` | `-tb` | prefill | decode | hash |
|---:|---:|---:|---:|---|
| **8** | **16** | 2428.5 | **41.04** | reference |
| 6 | 12 | 2411.0 | 40.72 | reference |
| 16 | 32 | 2431.2 | 40.57 | reference |
| 8 | 32 | 2417.7 | 40.41 | reference |

Every arm reproduces the reference hash, so all four are comparable as computations. **`-t 8 -tb 16`, the
existing default, is the best.**

## 2. What the flat profile says

**Prefill is flat at 2411-2431 across 6, 8 and 16 threads** - within measurement noise over a 2.7x range
of thread count. That means prefill is **not CPU-bound**: it is bound by the GPU work and the expert
transfer, and adding CPU threads cannot help.

Decode *degrades* with more threads (41.04 -> 40.57 at 16, 40.41 at 32 batch threads). That is contention:
more worker threads over the same expert set costs synchronisation without adding throughput.

The practical conclusion is that the thread count was already right, and the `--cpu-moe` experts are not
starved for cores - something else is the bottleneck.

## 3. Where the bottleneck is not

With all the levers now measured at the reference shape:

- **cache size**: optimised (4608, EXP-750);
- **ubatch / batch**: must stay 4096 for correctness;
- **threads**: optimised at 8/16, and prefill is not CPU-bound anyway;
- **VRAM**: 7804 MiB of 8 GiB-class budget, so ~250 MiB of headroom exists but larger caches OOM at
  ub 4096.

So the remaining cost is in the **work itself** - expert transfer and the GPU compute - not in a
misconfigured knob.

## 4. Recorded as a negative result

No default changes. `-t 8 -tb 16` stays, and this card exists so the sweep is not repeated: the thread
count has been measured across 6-16 threads with the reference hash confirmed in every arm.
