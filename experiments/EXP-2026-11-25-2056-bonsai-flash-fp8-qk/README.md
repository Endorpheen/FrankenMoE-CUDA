# EXP-2026-11-25-2056 — E4M3 QK in long Bonsai Flash prefill

**Experimental, not promoted.** Convert Q and the staged Q4 keys to
E4M3 and use FP8 Tensor Core QK with F32 accumulation. Preserve the
FP16 operands used for PV and the ordered FP32 merge. The opt-in host
path is scoped to the existing two-panel dispatch for at least 512
queries and 16,384 keys. It reuses the established K/V workspace.

The SM89 compiler uses 125 registers, 41,472 B dynamic shared memory,
and no spills. Clang 18 emits PTX 8.3 for this target. The build raises
the experimental file's version to 8.4 for FP8 MMA, which ptxas 12.6
accepts. This does not change the accepted EXP-2049 build.

## Diagnostic RTX 4070 component trace

Each sample includes K/V preparation and the fused attention call.
Inputs use fixed seeded F32 queries and Q4_0 cache rows derived from
seeded finite values. They are numerical fixtures, not traced model Q/K.

| Keys / queries | FP16 QK, ms | FP8 QK, ms | Relative L2 of output |
| --- | ---: | ---: | ---: |
| 257 / 31 | 0.0369 | 0.0362 | 2.29% |
| 8192 / 1024 | 7.2463 | 6.7322 | 2.29% |
| 32767 / 1023 | 31.8755 | 27.4620 | 2.28% |

All outputs are finite, including partial query and key panels.
The worst 95th-percentile row-relative L2 is 2.78%.

## Full Bonsai HTTP 32K/512

One manual sequential pair with the same executable and CUBIN,
the EXP-1977 raw request, context 36,864, batch/ubatch 4096/1024,
Q4_0 K/V, greedy and MTP off. PrismML is the separate saved control.

| Mode | Prefill, tok/s | Decode, tok/s | Client, s | Process VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A, FP16 QK | 745.81 | 32.65 | 59.610 | 8932 | 7457792 |
| B, E4M3 QK | 764.12 | 32.07 | 58.841 | 8932 | 7457416 |
| PrismML reference | 1076.60 | 47.63 | 41.174 | 8288 | 7651032 |

Prefill improves 2.46% in this pair. Decode falls about 1.79%,
although the trial dispatch does not run with single-token decode.
There is insufficient evidence to assign a cause to the decode
difference. All 512 IDs and response text match across both arms
and PrismML. Process swap is zero for A and B. No expanded
independent model quality or fixed-history logit suite ran.
No Apex or Inkling model paths were changed.

Keep EXP-2049 as the accepted baseline. Prioritize a substantial PQ2
MMQ improvement before reprofiling the combined Flash and remaining
operations. Results from separate experiments, including gains on
a different candidate baseline, are not additive performance evidence.

## Reproduce

Use the EXP-2049 local build and its frozen device image.

```bash
exp=experiments/EXP-2026-11-25-2056-bonsai-flash-fp8-qk
out=build/bonsai-flash-fp8-qk-final-2056
base=build/bonsai-flash-fused-padding-2049
bash "$exp/source/build.sh" "$out" "$base/pipeline.cl"
bash "$exp/source/build-http.sh" "$out" "$base"
CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration/inventory.json" \
  "$base/package/cuda/frozen-goose.ptx" "$out/pipeline.cubin"
```

Set `BONSAI_CUDA_IMAGE="$out/pipeline.cubin"` and run the EXP-2049
HTTP profile with the 32K/512 request. Use `BONSAI_FLASH_FP8_QK=0`
for A and `BONSAI_FLASH_FP8_QK=1` for B.

[Component trace](data/component.json) and [full-request evidence](data/results.json).
