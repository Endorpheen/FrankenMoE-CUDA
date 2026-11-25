# EXP-2026-11-25-2065 — Full tiles on cumulative Bonsai FP16 MMQ

**Retained in the cumulative experimental checkpoint.** Apply the
EXP-2048 full-tile specialization to the EXP-2059 K32 partial
accumulation path. Its dedicated input widths are 5,120 and 17,408.
Only complete 64×128 output tiles use the specialized entries.
Other widths and tails use the previous dispatch. The K32
FP16 accumulator segments, FP32 sums and per-output order are
unchanged. This is exact only relative to the cumulative arithmetic.

Both specialized entries use 80 registers, no spills, and 27,648 B
shared memory, versus 94 registers for the previous path. CUDA's
residency calculator reports three instead of two active blocks per
SM. This is a resource bound, not measured occupancy.

Three real full matrices produce bitwise identical outputs. A
diagnostic sample per path, including the common pack, records
1.777 → 1.635 ms for QKV, 3.018 → 2.740 ms for FFN up and
3.241 → 2.903 ms for FFN down. These are isolated projection
samples rather than aggregate throughput evidence.

## RTX 4070, Bonsai HTTP 32K/512

One manual sequential pair, identical executable and image, raw
EXP-1977 request, context 36,864, batch/ubatch 4096/1024, Q4_0 K/V,
greedy, MTP off. Both arms enable the EXP-2059 FP16 partials.
PrismML is the separate saved reference.

| Mode | Prefill, tok/s | Decode, tok/s | Client, s | Process VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A, cumulative control | 814.28 | 32.70 | 55.893 | 8932 | 7456832 |
| B, full tiles | 845.33 | 32.67 | 54.430 | 8932 | 7457684 |
| PrismML reference | 1076.60 | 47.63 | 41.174 | 8288 | 7651032 |

Prefill improves 3.81%, from 40.242 to 38.764 s. The complete request
saves 1.463 s. All 512 IDs and response text match across both arms
and PrismML. Process swap is zero. No new fixed-history logits were
captured. The working EXP-2049 and other model artifacts are unchanged.
The EXP-2059 numerical quality risk is retained, including the
18/63 → 13/63 CodeTrace result from its earlier quality fixture.
Expanded combined quality is still required before promotion.

The experimental prefill gap to the saved PrismML run is 8.327 s.
This follows one paired observation and is not an additive estimate.
The updated [checkpoint registry](../bonsai-checkpoints.json)
preserves both lines and the EXP-2059 provenance.

## Reproduce

Use the EXP-2057 device and host source that produced EXP-2059. Set
`FRANKENMOE_CHECKPOINTS` to the saved build tree.

```bash
exp=experiments/EXP-2026-11-25-2065-bonsai-cumulative-full-tiles
out=build/bonsai-cumulative-full-tiles-2065
base=build/bonsai-fp16-acc-2057
bash "$exp/source/build.sh" "$out" "$base/pipeline.cl"
bash "$exp/source/build-http.sh" "$out" "$base"
CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration/inventory.json" "$out/pipeline.cubin"
```

Set `BONSAI_MMQ_HALF_ACC=1` in both arms. Set
`BONSAI_MMQ_HALF_FULL=0` for A and omit it for B. Use the fixed
EXP-2059 HTTP profile, candidate CUBIN and frozen PTX.

[Component trace](data/component.json) and [full-request evidence](data/results.json).
