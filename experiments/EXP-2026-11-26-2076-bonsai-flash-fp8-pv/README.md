# EXP-2026-11-26-2076 — FP8 PV on cumulative Bonsai Flash

**Retained in the cumulative experimental checkpoint.** Add only
the FP8 V stage and FP8 QK/PV tile to the EXP-2073 image. The
89 retained machine-code sections are byte-identical.
QK already uses E4M3. Encode probabilities scaled by 256
and V in E4M3, compute a FP32-accumulating FP8 PV product, and
undo the power-of-two scale before ordered FP32 merge.
Mask, denominators, and cumulative MMQ are unchanged. Only the
existing long-context FP8 panel scope is affected.

## Component evidence

Three seeded Q and Q4 K/V fixtures include partial tiles.
All results are finite. One diagnostic sample per path on RTX 4070,
including K and V staging, in ms:

| Keys / queries | Existing FP16 PV | E4M3 PV | Relative L2 of output |
| --- | ---: | ---: | ---: |
| 257 / 31 | 0.0400 | 0.0325 | 3.58% |
| 8192 / 1024 | 7.681 | 4.463 | 3.53% |
| 32767 / 1023 | 26.367 | 18.493 | 3.55% |

The long component saves 29.86%, above its preregistered 10%
threshold. The FP8 PV tile uses 124 registers, 41,472 B dynamic
shared memory, and no spills. These are seeded component results,
not a model-level quality gate or a projected request gain.

## RTX 4070, Bonsai HTTP 32K/512

One manual sequential pair with the same executable and CUBIN.
Both enable the entire EXP-2073 checkpoint. Only FP8 PV changes.
Exact raw EXP-1977 input IDs, context 36,864, batch/ubatch 4096/1024,
Q4_0 K/V, greedy and MTP off. PrismML is the separate saved control.

| Mode | Prefill, tok/s | Decode, tok/s | Client, s | Process VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A, FP16 PV | 870.98 | 32.81 | 53.221 | 8932 | 7457544 |
| B, E4M3 PV | 912.33 | 32.74 | 51.545 | 8932 | 7456808 |
| PrismML reference | 1076.60 | 47.63 | 41.174 | 8288 | 7651032 |

Within this pair, prefill saves 1.705 s (4.75% throughput)
and the full request saves 1.676 s. Decode throughput is within
0.20% of the control and comparable to the prior EXP-2073
receipt. All 512 IDs and response text match across both arms
and PrismML. Process swap is zero. No new fixed-history logits
or expanded quality runs were performed.

The cumulative prefill gap to PrismML is 5.480 s. Stable
EXP-2049 and other model artifacts remain unchanged.
Reprofile full 32K/512 before assigning new priorities.
Combined long-context quality remains open for stable promotion.

## Reproduce

Use the EXP-2056 FP8 QK device source and EXP-2073 combined build.
Set `FRANKENMOE_CHECKPOINTS` to the saved build tree.

```bash
exp=experiments/EXP-2026-11-26-2076-bonsai-flash-fp8-pv
component=build/bonsai-flash-fp8-pv-2076
out=build/bonsai-combined-fp8-pv-2076
bash "$exp/source/build.sh" "$component" build/bonsai-flash-fp8-qk-final-2056/pipeline.cl
bash "$exp/source/build-combined.sh" "$out" build/bonsai-combined-q8-full-2073 "$component"
```

Use the saved inventory with `source/check.py` and the EXP-2073 frozen
PTX. For HTTP preserve all EXP-2073 flags and use the exact raw
32K/512 request. Toggle `BONSAI_FLASH_FP8_PV=0` for A and
`BONSAI_FLASH_FP8_PV=1` for B.

[Component trace](data/component.json), [device-code identity](data/device-code.json),
and [full-request evidence](data/results.json).
