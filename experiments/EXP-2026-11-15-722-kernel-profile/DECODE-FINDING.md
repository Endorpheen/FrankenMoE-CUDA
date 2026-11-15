# The measured defect: Goose flash attention scales linearly with context in decode

Follow-up to the profile in this directory. `perf` is blocked, `nsys` is not, so one 32K/512
request was profiled and the CUPTI tables queried directly.

## Splitting the profile by phase

GPU activity is unambiguous in time:

```
0-12 s    prefill        2,420 kernels    12.6 ms busy
63-77 s   decode        21,057 kernels   7,933 ms busy
```

So the earlier "55% Goose" figure was a blend. In the **decode** window specifically:

| seconds | calls | avg ms | kernel |
|---:|---:|---:|---|
| 2.363 | 2,928 | 0.807 | `mul_mat_q<type12>` ggml quantised |
| **1.419** | **80** | **17.742** | **`fg175m_kernel` Goose flash attention** |
| 0.744 | 464 | 1.603 | `mul_mat_q<type14>` |
| 0.641 | 240 | 2.673 | `fg174m_kernel` Goose GDN MMA |
| 0.548 | 936 | 0.585 | `fg181m_kernel` Goose MM ids |

**`fg175m` is 17.9% of decode GPU time** - 1.419 s of 7.933 s - from only 80 calls. Eight calls per
attention layer, ten attention layers.

## The defect

Its duration is not uniform. It **grows with context length**:

| time in decode | avg ms per call |
|---|---:|
| 63.7 s | 4.4 |
| 64.2 s | 3.9 |
| 65.7 s | 7.6 |
| 67.1 s | 12.4 |
| 69.0 s | 14.9 |
| 76.6 s | **34.5** |

A factor of **8.8x** between the first and the last calls of one 512-token generation.

Distribution over all 90 calls:

| bucket | calls | total ms |
|---|---:|---:|
| <1 ms | 10 | 1.7 |
| 1-5 ms | 10 | 40.0 |
| 5-15 ms | 28 | 308.3 |
| 15-25 ms | 22 | 455.1 |
| **25 ms+** | **20** | **615.9** |

**20 calls out of 90 (22%) account for 616 ms of 1419 ms - 43% of the kernel's time.** This is not a
uniformly slow kernel; it is a kernel whose cost is concentrated in its slowest invocations.

## Why this matters more than the matmul

The plan was to optimise the quantised matmul. But:
- the matmul is native ggml, already the substrate's tuned MMQ path with 128-wide tiles;
- `fg175m` is **Goose code we own**, so improving it advances both speed and migration;
- its cost is context-proportional, which means it will get **worse** at longer generation, not
  better - and longer generation is exactly where the reference beats us most (41.94 at 512 versus
  39.6 at 128).

## What is not yet established

Whether the growth is inherent to attention (it must touch more keys as context grows, so some
growth is expected and correct) or whether it is worse than necessary. Attention work grows with
context by definition; the question is whether this implementation grows faster than it must.

The way to answer it is a differential measurement: run the **native** flash attention kernel on
the same inputs and compare per-call duration. The integrated runtime has both paths available, so
this is measurable without a rewrite - which is the next step before any Goose change.

## The rule this follows

Measure the bottleneck first, then move or optimise it on Goose, then verify. Not the reverse. A
first change may well make it slower, and that is acceptable as long as the checkpoints hold and
the reason is understood.
