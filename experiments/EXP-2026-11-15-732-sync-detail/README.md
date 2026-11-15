# EXP-2026-11-15-732 — which synchronisations actually cost time: 321 calls hold 75% of the stall

Base: `ee197f25`. EXP-731 found 12.6 `cudaStreamSynchronize` per token costing 72% of the decode
window. This mines *which* of them cost the time, using the durations the profile already contains.

## 1. The distribution over the whole run

| duration | calls | estimated time | share of stall |
|---|---:|---:|---:|
| **>10 ms** | **321** | **~8.0 s** | **75%** |
| 1-10 ms | 738 | ~2.2 s | 21% |
| 100 us - 1 ms | 1,628 | ~0.5 s | 5% |
| 10-100 us | 142 | ~0.004 s | 0% |
| <10 us | 3,646 | ~0.007 s | 0% |

Total measured: 12.30 s across 6,475 syncs.

**The counts are misleading on their own.** 3,646 of the 6,475 calls (56%) cost essentially nothing -
they are calls on an already-idle stream, returning in under 10 us. Nine calls in ten cost under
1 ms. The stall lives in **321 calls out of 6,475, which is 5% of the calls holding 75% of the time.**

## 2. What that rules out

The long syncs are **not** one per layer: 321 long calls across 512 tokens is 0.63 per token, and
across 40 layers per token it is 0.016 per layer. Whatever is stalling, it is neither the layer loop
(which would give 40 per token) nor a clean per-token boundary (which would give 1).

Uniform distribution in time - about 12 long syncs per second of decode - matches the earlier gap
analysis, where gaps of 220-300 ms appeared every 1.75 s. Both are the same phenomenon seen two ways.

## 3. What is consistent with 321 long stalls

At roughly one every 1.6 tokens, and given that the expert cache performs 2,153 device-to-host copies
on the same stream, the most likely candidate is the **expert cache's fill/drain boundary**: when the
cache needs memory back it must wait for work already queued, which is exactly when a synchronisation
becomes expensive. The cheap syncs are the ones taken when nothing is in flight.

That is a hypothesis, and it is testable: the DtoH copies mark cache events, so their timestamps can
be correlated against the long syncs. If the long syncs cluster immediately after DtoH activity, the
cache is the cause.

## 4. The correlation was tested, and the hypothesis is wrong

Tested immediately, since it needed no server run: do the long syncs follow device-to-host activity?

```
long syncs >10 ms                                 322
DtoH copies                                    21,545
long syncs preceded within 6 ms by a DtoH copy      4  (1%)
```

**1%.** The expert cache is not the cause. 318 of the 322 long stalls happen with no readback
anywhere near them, so whatever the CPU is waiting for, it is not cache eviction.

That leaves the sampler/logits path and the graph boundary as candidates, and the same
timestamp-correlation test can separate them - but it needs a marker for the token boundary, which the
profile does not have. Adding one is the next step: without knowing where each token starts, "before
or after sampling" cannot be answered from timestamps alone.

## 5. What precedes a long sync: one and a half tokens of queued work

Correlating the 322 long syncs against the kernel stream that precedes them:

| observation | value |
|---|---:|
| kernels per token (measured) | 41.1 |
| kernels in the 50 ms before each long sync | **62** |
| 62 kernels expressed in tokens of work | **1.51** |
| long syncs per token | 0.63, i.e. one per 1.59 tokens |
| `fg166` (CUDA activations) calls per long sync | 6.8 |

**One long synchronisation every 1.59 tokens, each preceded by 1.51 tokens of queued kernel work.**
The two numbers agree, which is what makes this a coherent picture rather than a coincidence: the CPU
is waiting for a batch of roughly one and a half tokens to drain.

So the stall is not attached to a layer boundary, a token boundary or a cache event. It is attached to
**how much work is queued between two waits** - the CPU batches ~1.5 tokens, blocks, and repeats. That
also explains why 56% of all synchronisations are free: those are the calls taken when nothing is in
flight, between batches.

And it explains why the kernel most often preceding a long sync is `fg166` (an activation kernel): the
sync lands after the last activation of the batch, whatever layer that happens to be, which is why the
preceding kernel is nearly always the same one rather than one per layer.

## 6. What I am not doing

Not changing any synchronisation. The cheap ones are cheap and harmless; the expensive ones are
presumably where correctness requires a wait. Removing a required wait would be a correctness bug, not
an optimisation, and the failure mode - reading a tensor before it is ready - produces wrong output
rather than slow output.

The next step is the correlation measurement above, which costs nothing and needs no change.

## 7. Cost

Analysis of the existing profile only. No server run, no rebuild.
