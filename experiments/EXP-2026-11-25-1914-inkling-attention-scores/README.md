# EXP-2026-11-25-1914: Parallelize single-query Inkling relative attention scores

For one-query decode, distribute the independent relative corrections
of a 32-key tile across GPU lanes. Each key retains the reference
FP32 products and FP64 accumulation order. The original row leader
still computes the ordered maximum, exponentials and denominator.
The existing MMA products, masks and key partitions are unchanged.
Batched prefill keeps its original execution path. The isolated flag
is `I2_FLASH_PARALLEL_SCORES=1` for B and `0` for A.

The same binary ran both arms on RTX 4070, with the frozen 512/16 input,
context 1024, Q4_0 KV, 512 ubatch, 384 MiB activation arena and the previously
verified worker team, sampler and dense Q8 tile. Each launch had one
cold expert-cache request and one repeat. The A–B–B–A means were:

| Metric | Baseline | Parallel scores |
| --- | ---: | ---: |
| Cold prefill, tok/s | 146.78 | 146.32 |
| Warm prefill, tok/s | 179.10 | 183.05 |
| Cold decode, tok/s | 10.52 | 13.90 |
| Warm decode, tok/s | 10.92 | 14.09 |

Decode improved in both pairs. All 1086 fingerprints per server,
generated IDs, text and expert traffic matched exactly. Board peaks
were 9811–9818 MiB and swap counters did not grow. Process peak RSS
varied by less than 1.5 MiB. The result covers the short Inkling
profile rather than the full-context or combined Apex gates.

Apply the Goose and OpenCL patches to the frozen Inkling online-softmax
and Q4 cache Flash sources. Apply the driver patch to the native KV
header from EXP-1913. The build helper targets the sibling experiment
layout and preserves the frozen host archive and SM89 device target.
Use the prior 512/16 regression fixture. Raw paired metrics are in
`results.json`. No production configuration or Apex binary changed.
