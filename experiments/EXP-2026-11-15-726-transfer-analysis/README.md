# EXP-2026-11-15-726 — the transfer cost is structural and completely unoverlapped

Base: `df8e262c`. Three measurements on the profile of the pinned configuration, answering the
question the last card left open: is the 84 GiB of expert traffic reducible, or inherent?

## 1. Cache hit rate, measured

| quantity | value |
|---|---:|
| host-to-device copies in decode | 77,581 |
| tokens generated | 512 |
| **copies per token** | **152** |
| expert uses per token (40 layers x 8 active, if all missed) | 320 |
| **implied hit rate** | **53%** |

Distinct copy sizes show the expert granularity clearly: 590,336 B (18,460 copies), 589,824 B
(14,278), 1,180,160 B (8,995), 1,769,984 B (4,475) - i.e. 576 KiB multiples, one expert's Q4_K
tensors.

## 2. The cache is already doing better than random

One expert is 1.69 MiB (three tensors of 512x2048 at Q4_K). 4352 MiB therefore holds about 2,579
experts across 40 layers, or 64 of 256 experts per layer. A **random** access would hit 25.2% of the
time. Measured: **53%**. So there IS residency state and it is working - roughly twice as well as
blind caching.

## 3. But no cache size can fit the model

Full residency of all experts would need **16.9 GiB**; the GPU offers 12 GiB total and the cache
ceiling here is 4.2 GiB - **four times short**. Expert traffic is therefore structural for Apex on
this device, not a tuning failure. Adding cache can only move the hit rate along a curve that tops
out far below 100%.

## 4. The finding that actually matters: zero overlap

Copy time in the decode window: 4.24 s. Kernel time: 7.93 s. Overlap between them, computed from the
CUPTI intervals:

```
copy total 4.24 s, overlapping kernels 0.00 s = 0%
```

**Not one microsecond of the copying overlaps compute.** The copies and the kernels alternate on the
device: the GPU computes, then waits, then computes. That is why the decode window is 17 s while
kernel work is 7.93 s - the rest is transfer with the GPU idle.

Only two CUDA streams are in use for 189,839 copies, which is consistent with a serialised path.

## 5. So there are two independent wins available, and only one needs less traffic

| lever | what it needs | expected effect |
|---|---|---|
| raise the hit rate | more VRAM than exists | limited by the 12 GiB device |
| **overlap transfer with compute** | pipelining on a second stream | up to 4.24 s of a 17 s window, i.e. potentially the whole gap |

The second does not reduce traffic at all. It hides it. If a layer's expert copy for token N+1 were
issued while token N computes, the 4.24 s of transfer would largely disappear from the critical path
without touching hit rate, cache size or correctness.

That is a scheduling change in the expert path, not a numerical one, which makes it the safest class
of optimisation available: if it is wrong, the numbers get worse and it is reverted; no answer can
change.

## 6. What is needed to test it

The runtime must issue expert copies on a stream that is not synchronised with the compute stream.
Whether the current path already has such a hook, or would need one, is answerable from the code -
`fg262a_run` and `fg263c_fill_bytes` are the Goose entry points that decide the copies, and they are
currently not called at all (EXP-725). Wiring them in is the prerequisite for changing how the
copies are scheduled.

## 7. Not claimed

That overlap will yield the full 4.24 s. PCIe bandwidth, dependency order between layers and the
GPU/CPU split all limit what can be hidden. What is established is that currently *none* of it is
hidden, which is the measurable starting point.
