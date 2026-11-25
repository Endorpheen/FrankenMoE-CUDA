# EXP-2026-11-25-2070 — Combine Bonsai FP8 QK and the cumulative MMQ path

**Retained in the cumulative experimental checkpoint.** Add only
the three EXP-2056 E4M3 Flash entries to the EXP-2067 PTX.
The assembler emits all 84 retained machine-code sections
byte-identically. The host selects FP8 QK for the same long-panel
scope as EXP-2056, at least 512 queries and 16,384 keys.
It keeps FP16 PV and ordered FP32 merge. Existing MMQ kernels
and profile choices remain active in both arms. This is a
changed-precision Flash experiment, not an exact stable variant.

## RTX 4070, Bonsai HTTP 32K/512

One manual sequential pair with the same executable and CUBIN.
Both enable the EXP-2067 full-tile FP16 partial MMQ. Raw EXP-1977
request, context 36,864, batch/ubatch 4096/1024, Q4_0 K/V,
greedy, MTP off. PrismML is the separate saved reference.

| Mode | Prefill, tok/s | Decode, tok/s | Client, s | Process VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A, EXP-2067 with FP16 QK | 846.26 | 32.69 | 54.374 | 8932 | 7456884 |
| B, combined with FP8 QK | 867.99 | 32.72 | 53.392 | 8932 | 7457496 |
| PrismML reference | 1076.60 | 47.63 | 41.174 | 8288 | 7651032 |

Within this one pair, prefill saves 0.969 s (2.57% throughput)
and the full request saves 0.983 s. All 512 IDs and response text
match across A, B and PrismML. Process swap is zero. This is the
measured benefit of the combined configuration. It is not the sum
of individual candidate deltas.

The experimental prefill gap to PrismML is 7.315 s. No new
fixed-history logits or expanded quality suite ran. The earlier
EXP-2059 CodeTrace risk still applies to inherited MMQ, and
FP8 requires its own applicable long-context quality coverage
before stable promotion. Stable EXP-2049 and other model paths
are unchanged. Continue prioritizing MMQ and use the latest
combined checkpoint for new experimental comparisons.

## Reproduce

Use the EXP-2067 cumulative build and the exact EXP-2056 PTX
build. Set `FRANKENMOE_CHECKPOINTS` to the saved build tree.

```bash
exp=experiments/EXP-2026-11-25-2070-bonsai-cumulative-fp8-qk
out=build/bonsai-cumulative-fp8-2070
bash "$exp/source/build.sh" "$out" build/bonsai-mmq-width6144-2067 \
  build/bonsai-flash-fp8-qk-final-2056
```

Set `BONSAI_MMQ_HALF_ACC=1` in both arms. Toggle
`BONSAI_FLASH_FP8_QK=0` for A and `BONSAI_FLASH_FP8_QK=1` for B.
Use the fixed EXP-2067 profile, combined CUBIN and frozen PTX.

[Device-code identity](data/device-code.json) and [full-request evidence](data/results.json).
