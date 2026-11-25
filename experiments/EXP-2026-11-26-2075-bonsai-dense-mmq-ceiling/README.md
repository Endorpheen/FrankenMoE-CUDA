# EXP-2026-11-26-2075 — Dense FP16 GEMM diagnostic for Bonsai PQ2

**Rejected as a runtime route.** Expand the stored PQ2 matrix into
the same dense FP16 operands and use the local cuBLAS 12.6
`cublasGemmEx` heuristic with FP16 inputs, FP32 accumulation and output.
Disallow reduced-precision reduction. The original FP16 activation pack
is common. This puts a dense library implementation behind the exact
stored weights, without adding a production dependency. Its reduction
order differs from the cumulative FP16-partial path.

All three fixtures match the sampled independent CPU FP16-operand
multiplication within the diagnostic tolerances. The zero activation
row gives zero. Relative L2 versus cumulative EXP-2073 is about 0.033%.

## RTX 4070 diagnostic component trace

One CUDA trace sample per kernel on actual weights and 1024 seeded
activation rows. Times are milliseconds. Both totals include the
common activation pack. The dense total includes expansion.

| Matrix | EXP-2073, pack + MMQ | Expand | Dense GEMM alone | Dense total | Expanded weight scratch |
| --- | ---: | ---: | ---: | ---: | ---: |
| QKV | 1.633 | 0.225 | 1.887 | 2.157 | 100 MiB |
| FFN up | 2.763 | 0.579 | 3.225 | 3.868 | 170 MiB |
| FFN down | 2.907 | 0.423 | 3.355 | 3.990 | 170 MiB |

The chosen dense implementation loses even before adding expansion.
This observation covers the default FP32-accumulating cuBLAS heuristic,
not every library algorithm or a hardware performance ceiling.
The scratch column counts the explicit expanded weights in the
component fixture, not peak process VRAM or internal library workspace.

There is no case for adding this dense dispatch or its scratch now.
A new attempt would need a different mechanism or a faster measured
GEMM that covers its preparation cost. No end-to-end rate, model
quality or other model paths were changed or measured. Stable
EXP-2049 and cumulative EXP-2073 remain the reference checkpoints.

## Reproduce

Use the EXP-2067 device source and the actual EXP-2073 CUBIN.
The diagnostic requires the local CUDA 12 cuBLAS library.
Set `FRANKENMOE_CHECKPOINTS` to the saved build tree.

```bash
exp=experiments/EXP-2026-11-26-2075-bonsai-dense-mmq-ceiling
out=build/bonsai-dense-mmq-ceiling-2075
bash "$exp/source/build.sh" "$out" build/bonsai-mmq-width6144-2067/pipeline.cl
CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 LD_LIBRARY_PATH=/usr/local/cuda/lib64 \
  python3 "$exp/source/check.py" "$FRANKENMOE_CHECKPOINTS/bonsai-integration" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration/inventory.json" "$out/pipeline.cubin" \
  build/bonsai-combined-q8-full-2073/pipeline.cubin
```

[Measured samples and numerical checks](data/component.json).
