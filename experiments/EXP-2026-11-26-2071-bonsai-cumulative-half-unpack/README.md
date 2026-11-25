# EXP-2026-11-26-2071 — Exact FP16x2 weight unpack for cumulative Bonsai FFN down

**Retained in the cumulative experimental checkpoint for FFN down.**
Test EXP-2022's verified half LUT and half2 unpack methods on the
full-tile FP16 partials used by EXP-2070. Both produce the same
FP16 operands for finite PQ2 scales. Preserve MMA ordering and
partial-sum boundaries. Scope the combined host change to full
17,408-to-5,120 FFN down tiles. The working EXP-2049 and other
model paths remain unchanged.

## Component gate

Bitwise outputs on four actual GGUF matrices. Single RTX 4070
diagnostic CUDA samples include the common activation pack.

| Matrix / 1024 tokens | Current, ms | LUT, ms | FP16x2, ms |
| --- | ---: | ---: | ---: |
| QKV | 1.629 | 1.639 | 1.614 |
| SSM output | 1.014 | 1.069 | 0.997 |
| FFN up | 2.729 | 2.740 | 3.277 |
| FFN down | 3.466 | 2.930 | 2.898 |
| FFN down, reverse order | 3.319 | 3.651 | 2.884 |

The first FFN down control is slower than the approximately 2.91 ms
historical component run. The one targeted reverse observation
also favors FP16x2, and does not support the LUT. Full-tile FP16x2
uses 79 registers instead of 80, the same 27,648 B shared memory,
and no spills. These isolated times are not predicted request gains.

## RTX 4070, Bonsai HTTP 32K/512

One manual sequential pair with the same executable and CUBIN.
Both enable the entire EXP-2070 checkpoint, including FP8 QK.
Only the FP16x2 unpack for full FFN down tiles changes.
Raw EXP-1977 request, context 36,864, batch/ubatch 4096/1024,
Q4_0 K/V, greedy and MTP off. PrismML is the separate saved reference.

| Mode | Prefill, tok/s | Decode, tok/s | Client, s | Process VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A, EXP-2070 control | 859.41 | 32.71 | 53.774 | 8932 | 7458252 |
| B, FP16x2 FFN down | 870.40 | 32.76 | 53.272 | 8932 | 7457632 |
| PrismML reference | 1076.60 | 47.63 | 41.174 | 8288 | 7651032 |

Within this one pair, prefill saves 0.481 s (+1.28%) and the
request saves 0.502 s. Relative to the earlier separate EXP-2070
candidate at 867.99 tok/s, the new value is about 0.28% higher.
Neither comparison establishes a repeatable gain magnitude.
All 512 IDs and response text match across both arms and PrismML.
Process swap is zero. No new fixed-history logits were collected.
The full combined quality gate for inherited arithmetic changes
remains open. The cumulative prefill gap to PrismML is 7.211 s.

## Reproduce

Use the EXP-2067 device source and EXP-2070 combined build.
Set `FRANKENMOE_CHECKPOINTS` to the saved build tree.

```bash
exp=experiments/EXP-2026-11-26-2071-bonsai-cumulative-half-unpack
component=build/bonsai-cumulative-half-unpack-2071
out=build/bonsai-combined-half2-down-2071
bash "$exp/source/build.sh" "$component" build/bonsai-mmq-width6144-2067/pipeline.cl
bash "$exp/source/build-combined.sh" "$out" build/bonsai-cumulative-fp8-2070 "$component"
```

Use the saved inventory with `source/check.py`, or add
`--down-only-reverse` for the targeted diagnostic. For HTTP set
`BONSAI_MMQ_HALF_ACC=1` and `BONSAI_FLASH_FP8_QK=1` in both arms.
Set `BONSAI_MMQ_HALF2_DOWN=0` for A and omit it for B. Keep the
fixed EXP-2070 HTTP profile and its frozen PTX.

[Component trace](data/component.json), [reverse observation](data/reverse.json),
and [full-request evidence](data/results.json).
