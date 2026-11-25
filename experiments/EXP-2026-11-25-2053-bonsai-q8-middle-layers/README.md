# EXP-2026-11-25-2053 — Bonsai Q8 gate/up in layers 32–47

**Experimental, not promoted.** Add the two-scale Q8 path from
EXP-2052 only to gate/up in zero-based layers 32–47. Preserve
FP16 in layers 0–31, the already accepted one-scale Q8 in layers
48–63 and the existing decode dispatch. The candidate reuses
the EXP-2052 CUBIN and changes only the Bonsai host selection.
Enable it in this experiment with `BONSAI_FFN_Q8_64=1`.

## Independent quality

Four established EXP-1994 cases, greedy with normal EOG. All
37 sampled full-vocabulary rows on identical forced history have
matching top-1 and at least 9 of 10 shared top-10 IDs. Maximum
sampled absolute logit difference is 0.5334 (scheduler).

| Case | Output IDs A/B | Identical prefix | Independent score |
| --- | ---: | ---: | --- |
| Code trace | 258/262 | 199 | 18/63 → 17/63 correct cells |
| Ledger | 23/23 | 23 | Both incorrect |
| Scheduler | 63/63 | 63 | Both incorrect |
| Incident extraction | 47/47 | 47 | Both correct |

On the exact shared 199-token history at the first difference,
the control selects ID 16 and the candidate chooses ID 20.
Maximum absolute full-vocabulary delta is 0.3486 at that step.
The small code-trace suite and one lost scored cell do not
establish the size of the broader quality effect. Matching the
long throughput fixture below does not resolve that uncertainty.

## RTX 4070, Bonsai HTTP 32K/512

One manual sequential pair with the same executable and image,
the EXP-1977 raw inputs, context 36,864, batch/ubatch 4096/1024,
Q4_0 K/V, greedy and MTP off. The PrismML row is the saved,
separate control in the same workload.

| Mode | Prefill, tok/s | Decode, tok/s | Client, s | Process VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| FrankenMoE A | 744.98 | 32.50 | 59.731 | 8932 | 7458032 |
| FrankenMoE B | 764.08 | 32.71 | 58.530 | 8932 | 7456956 |
| PrismML reference | 1076.60 | 47.63 | 41.174 | 8288 | 7651032 |

Candidate prefill saves **1.100 s (+2.56% throughput)** in this
pair. All 512 IDs and response text match across A, B and PrismML.
Process swap is zero for A and B. The observation shows a viable
prefill cost reduction, while the separate quality regression
signal and narrow coverage leave promotion open. Keep EXP-2049 as
the working path. Retest this direction only with a new hypothesis
for the sensitive layers, or an expanded independently scored
quality set targeted at the observed discrepancy. No other model
binary or workload was changed.

## Reproduce

Use the EXP-2049 build and the EXP-2052 CUBIN. Point
`FRANKENMOE_CHECKPOINTS` to the saved build tree.

```bash
exp=experiments/EXP-2026-11-25-2053-bonsai-q8-middle-layers
out=build/bonsai-q8-middle-2053
bash "$exp/source/build-quality.sh" "$out" build/bonsai-flash-fused-padding-2049 \
  build/bonsai-q8-two-scales-final-2052
bash "$exp/source/build-http.sh" "$out"
```

Use the EXP-2052 quality and replay harness with this build
directory, without the `quality-fixture` suffix. For HTTP set the
CUDA image to `$out/pq2-int8.cubin`, use the EXP-2049 profile and
the exact 32K/512 request. Set `BONSAI_FFN_Q8_64=0` for A and
`BONSAI_FFN_Q8_64=1` for B.

[Full-request results](data/results.json), [quality](data/quality.json),
and [first divergence](data/history-199.json).
