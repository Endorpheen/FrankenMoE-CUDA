# EXP-2026-11-15-730 — the transfer is already at PCIe speed; my earlier estimate was wrong

Base: `d096d17c`. This card corrects a number I published one card earlier, and closes the bandwidth
direction.

## 1. The correction

EXP-729 quoted "54.7 us per 590 KB = 10.8 GB/s", inferred that pinned memory was not in use, and
proposed enabling it. That inference was wrong.

The 54.7 us was an **average over all copy sizes**, and the average is dragged down by 32,664 tiny
copies. Measured per size class:

| size class | copies | avg us | effective bandwidth |
|---|---:|---:|---:|
| <64 KiB | 32,664 | 0.4 | 3.5 GB/s |
| 64-300 KiB | 1,071 | 10.2 | 16.1 GB/s |
| 300-700 KiB | 101,957 | 29.0 | 20.3 GB/s |
| >700 KiB | 54,147 | 76.5 | 21.3 GB/s |

**20-21.7 GB/s is pinned-memory speed on PCIe 4.0 x16.** The transfers are already using registered
host memory. There is nothing to gain from `GGML_CUDA_REGISTER_HOST`.

Confirmed empirically: setting `GGML_CUDA_REGISTER_HOST=1` gives **39.66 decode against 39.86
baseline** - parity, within noise, as expected if it was already effectively in use.

## 2. The small copies are not a target either

32,664 copies below 64 KiB take 0.4 us each - **0.013 s in total, 0.3% of copy time**. They are
launch-overhead dominated (3.5 GB/s is the latency floor for a small transfer, not a bandwidth
problem). Batching them would save at most 0.013 s.

Real time is in the 300 KiB+ classes: 156,104 copies averaging 29-77 us = about 4.0 s.

## 3. What this closes

| hypothesis | verdict |
|---|---|
| transfers are pageable, pinned would double them | **wrong** - already 20-21.7 GB/s, pinned speed |
| small copies are wasteful | true but irrelevant - 0.3% of time |
| `GGML_CUDA_REGISTER_HOST` would help | measured parity (39.66 vs 39.86) |

So the 4.24 s of transfer is **the hardware's own limit for this volume over this link**. 86 GiB
through PCIe at 21 GB/s is ~4.1 s - which is what was measured. **The traffic cannot be made faster;
it can only be made to not block.**

That leaves exactly one lever, unchanged from EXP-729: put the copies on a separate stream so they
overlap compute instead of preceding it.

## 4. The arithmetic, stated once

```
86,421 MiB / 21 GB/s  = 4.1 s   <-- measured 4.24 s, so the link is saturated
decode window                                 17.0 s
kernel work                                    7.93 s
transfer                                       4.24 s
unaccounted (gap between kernels and copies)   4.8 s
```

The link is doing its maximum. Every remaining second is either compute, transfer at full speed, or
scheduling.

## 5. Cost

One server run for the `GGML_CUDA_REGISTER_HOST` measurement; the rest is analysis of the existing
profile.
