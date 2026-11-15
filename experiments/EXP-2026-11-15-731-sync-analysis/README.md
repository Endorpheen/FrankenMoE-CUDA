# EXP-2026-11-15-731 — the real bottleneck: 12.6 stream synchronisations per token

Base: `e404c69b`. EXP-730 established the transfer runs at full PCIe speed and cannot be made faster.
This card finds where the remaining time actually goes, and it is not the link.

## 1. The decode window, fully decomposed

Merging the CUPTI kernel and copy intervals:

| component | seconds | share of 17 s |
|---|---:|---:|
| kernel busy | 7.93 | 46.7% |
| copy busy | 4.24 | 24.9% |
| **union (either)** | **12.17** | **71.6%** |
| **gap (neither)** | **4.83** | **28.4%** |

The union equals the sum exactly, which re-confirms zero overlap. But **28.4% of the window has
neither a kernel nor a copy running** - the GPU is genuinely idle.

## 2. What the CPU is doing during that idle time

Decomposed CPU-side CUDA API time in the same window: **13.58 s across 128,538 calls**.

| API | calls | seconds | avg |
|---|---:|---:|---:|
| **`cudaStreamSynchronize`** | **6,475** | **12.296** | **1,899 us** |
| `cudaMemcpyAsync` | 95,590 | 0.555 | 5.8 us |
| `cudaMallocHost` | 1 | 0.335 | 335 ms |
| `cudaMalloc` | 12 | 0.164 | 13.7 ms |
| `cudaLaunchKernel` | 22,641 | 0.116 | 5.1 us |
| `cudaGraphLaunch` | 1,640 | 0.055 | 33.5 us |

**72% of the decode window is spent inside `cudaStreamSynchronize`.** Everything else - launches,
memcpys, graph launches - is rounding error by comparison.

## 3. The arithmetic per token

```
6,475 syncs / 512 tokens        = 12.6 synchronisations per token
12.296 s      / 512 tokens      = 24.0 ms blocked per token
budget at 40 tok/s              = 25.0 ms per token
```

**The CPU is blocked 24.0 ms of every 25.0 ms.** The entire decode step is: queue work, block, GPU
drains the queue, GPU idles while the CPU resumes and refills, repeat - 12.6 times per token, i.e.
one synchronisation every 3.2 layers.

## 4. Why that also explains the 28% GPU idle

A synchronisation forces the GPU to finish *all* queued work before the CPU continues. After the
sync, the queue is empty and the GPU has nothing to do until the CPU has issued the next batch. That
refill is the 4.83 s of idle: it is not a slow kernel or a slow link, it is the pipeline emptying 12.6
times per token and having to be refilled.

## 5. This reframes every previous finding

| finding | reinterpretation |
|---|---|
| 4.24 s of expert transfer | at full PCIe speed; it is *in* the critical path because of the syncs |
| 0% overlap | a consequence of one stream plus frequent syncs, not an independent problem |
| 28% GPU idle | the queue emptying after each sync |
| cache-size tuning has little effect | the bottleneck is not bandwidth, it is 12.6 stalls per token |

Reducing synchronisations is therefore worth more than any kernel or transfer optimisation, and
unlike the stream-splitting change it does not touch numerics at all - it changes **when** the CPU
waits, not what is computed.

## 6. What is needed next, specifically

Find what calls `cudaStreamSynchronize` 12.6 times per token. Candidates, in order:
1. the expert-cache copy path - `impl::copy` runs inside the graph, and the observed 2,153 DtoH
   copies on the same stream suggest per-layer readbacks;
2. the sampler / logits path, which must read the last token's output back per token;
3. the scheduler's graph boundaries, if the graph is rebuilt per layer rather than per token.

Each is distinguishable by looking at *when* in the token the syncs occur - a measurement the profile
already contains and that has not been mined yet.

## 7. Not claimed

That fewer syncs will give the full 24 ms back. Some synchronisation is required for a token to be
emitted at all, and the DtoH readback for sampling is genuinely needed. What is established is that
12.6 per token is far more than a 40-layer model needs for that purpose, and that the mechanism costs
72% of the window.
