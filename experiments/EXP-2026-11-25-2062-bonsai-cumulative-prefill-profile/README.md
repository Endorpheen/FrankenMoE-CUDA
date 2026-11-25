# EXP-2026-11-25-2062 — Profile the cumulative Bonsai prefill checkpoint

**Diagnostic, no runtime change.** Profile the staged experimental
EXP-2059 package, with half-precision K32 MMQ partials enabled by
its settings, and re-use the stable EXP-2049 and PrismML captures
from EXP-2050. RTX 4070, identical uncached 32K input IDs, 16
generated tokens, Q4_0 K/V, context 36,864, batch/ubatch 4096/1024,
greedy, MTP off. All 16 returned IDs match across the three traces.

| Prefill activity, ms | PrismML | Stable EXP-2049 | Experimental EXP-2059 |
| --- | ---: | ---: | ---: |
| GPU timeline span | 28,405.45 | 43,880.13 | 40,354.36 |
| Main matrix kernels and activation packing | 16,328.17 | 29,930.92 | 26,038.12 |
| Flash stages, tiles and merge | 4,064.58 | 8,022.59 | 8,312.95 |

The experimental trace records 11,776 launches of
`bonsai_pq2_cached_fp16_acc_k32` (23.329 s) and no calls to the
stable cached K64 kernel in the prefill bracket. Matrix and packing
span totals are kernel time sums, not exclusive allocations of
request latency. The Flash difference between these historical
captures is not scored as a regression or attributed to the
numerical path. A separate trace cannot replace paired results.

The first opportunity remains MMQ. The experimental line's matrix
group is 9.71 s slower than the PrismML trace, while Flash is 4.25 s
slower. Those cross-trace deltas are priorities, not additive
guaranteed savings. Any new PQ2 candidate must be checked against the
current cumulative line. Retain the stable EXP-2049 path and use
bitwise checks for claims that arithmetic is preserved.

The scored 32K/512 figures are carried forward from the two
checkpoint receipts. Stable gives 748.10 tok/s prefill and 32.70
tok/s decode. Experimental gives 815.16 and 32.71 tok/s in its
single recorded candidate run. Both report 8,932 MiB process VRAM
after the request. This profile does not change those results or
close the experimental quality gate.

## Reproduce

Use the staged EXP-2059 package and EXP-2050's diagnostic profile
with a single 32K/16 request. Export the Nsight report as
`trace.sqlite` and keep `response.json` beside it. Use the same
two saved references as the comparison:

```bash
python3 experiments/EXP-2026-11-25-2062-bonsai-cumulative-prefill-profile/source/attribute.py \
  PRISM_EXP2010_TRACE STABLE_EXP2049_TRACE CUMULATIVE_EXP2059_TRACE
```

[Measured kernel groups and trace hashes](data/attribution.json).
