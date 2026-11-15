# EXP-2026-11-15-721 — KV cache type: f16 is faster but changes the answer

Base: `8a9693c4`. Cache 4352 MiB, 512 tokens, three warmed runs per arm (f16 twice = six).

## 1. Result

| KV type | prefill | decode | range | VRAM | vs q4_0 | content sha |
|---|---:|---:|---|---:|---:|---|
| q4_0 (reference) | 2429.9 | 39.86 | 39.83-40.22 | 6784 | - | `c2ff42cc` |
| q8_0 | 2429.6 | 39.26 | 39.16-39.66 | 7712 | **-1.5%** | different |
| **f16** | 2462.6 | **40.33** | 39.88-40.69 | 8030 | **+1.2%** | `491d1d32` |

f16 confirmed twice on separate servers: 40.24 and 40.46 medians.

## 2. Why this is NOT adopted

**f16 changes the output.** The reference answer at 512 tokens hashes to `c2ff42cc`; the q4_0 arm
reproduces it exactly, and the f16 arm produces `491d1d32`. The text is still coherent and
on-topic - it is a different sample, not corruption - but it is not the reference answer.

Every acceptance criterion in this project is "output identical to the reference". f16 buys +1.2%
decode and +1.3% prefill for +1246 MiB of VRAM and a different answer. Under rule 19 that exchange
is favourable on speed and memory is affordable, but under the correctness rule it is a different
configuration that must be labelled as such, not an improvement to the reference one.

So f16 is recorded as a **candidate**, not adopted:
- it is the fastest configuration measured (40.33 median, best 40.69);
- it is the only arm that beat q4_0 outside the noise band;
- it does not reproduce the reference output.

If Igor accepts "a coherent answer, faster, with more VRAM" as the goal rather than "the reference
answer", f16 becomes the throughput configuration. Until then the pinned config is q4_0.

## 3. q8_0 is simply worse

39.26 decode at 7712 MiB: slower than q4_0 and 928 MiB more expensive. No trade, just a loss. The
KV quantisation is not the bottleneck it looked like - q4_0 already sits near the fast end, and
q8_0's extra bytes buy nothing.

## 4. What this says about the remaining 4.5%

If KV precision changes decode by only +1.2% while removing all KV quantisation work, then the
attention path is not where the 4-5% lives either. That leaves the quantised matmul kernels
themselves, which is where the next investigation goes.

## 5. Balance, per rule 19

| axis | q4_0 | f16 | change |
|---|---:|---:|---:|
| prefill | 2429.9 | 2462.6 | +1.3% |
| decode | 39.86 | 40.33 | +1.2% |
| VRAM | 6784 | 8030 | +1246 MiB (+18.4%) |
| RAM | 21.4 GiB | not sampled | - |
| content | `c2ff42cc` | `491d1d32` | **changed** |

All nine rows are in `benchmarks/experiment-history.jsonl`.
