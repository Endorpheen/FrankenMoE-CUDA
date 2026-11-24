# EXP-2026-11-24-1857 — Submit the first decode region after two kernels

Status: **not adopted**. The cumulative mailbox plus deferred-prefetch-fence
runtime remains at split 4/count 1. This experiment changed only the early
submission threshold on the same binary. It tested two kernels with one
submission, distinct from the earlier two-kernel/count-two point.

## Apex 32K/512 on RTX 4070

Context 262144, batch/ubatch 4096, Q4_0 KV and 6290 MiB expert cache.
Both arms used one warm-up followed by one measured request.

| Arm | Split/count | Prefill tok/s | Decode tok/s | Full seconds |
| --- | --- | ---: | ---: | ---: |
| Control | 4/1 | 3242.564 | 82.903 | 16.356719 |
| Candidate | 2/1 | 3251.217 | 82.380 | 16.372249 |

The candidate lost 0.63% decode and the complete request took 15.5 ms
longer. One pair does not establish the effect size. It provides no basis
to replace the accepted setting. All 512 IDs and text matched in both
warm-ups and measurements. Properties matched the cumulative baseline
byte for byte. System swap-out was zero. See [measurements](results.json)
for memory and raw run identifiers. No runtime code was changed.
