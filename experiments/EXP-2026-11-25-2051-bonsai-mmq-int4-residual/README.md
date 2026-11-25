# EXP-2026-11-25-2051 — INT4 rounding correction for Bonsai PQ2 MMQ

**Rejected at the component performance gate.** EXP-1996 used an INT8
main term and a second INT8 product for the rounding residual. This
candidate retains the main term, packs the residual in signed INT4 and
uses K64 INT4 MMA alongside K32 INT8 MMA. It rounds activations to FP16
first, uses one main scale per 128 values and uses a correction scale
of main scale divided by 14. It leaves the packed PQ2 weights on disk
unchanged. This is a different numerical path from the FP16 control.

The independent CPU block-scale oracle matches the sampled CUDA
results within 8.84e-7 absolute error across four real PQ2 weight
fixtures, including the 129-token and 10,239-row tail. Cases also check
an all-zero activation row. Relative L2 versus FP16 is 0.0459–0.0463%,
below the preregistered 0.10% component threshold. This does not
establish model-level functional equivalence.

## Diagnostic RTX 4070 trace

One CUDA capture of the same component assertions. Times add only the
activation pack and matrix kernel for each path, in milliseconds.

| Projection | Tokens | EXP-2049 FP16 | INT8 plus INT4 |
| --- | ---: | ---: | ---: |
| Layer 0 QKV, row tail | 129 | 0.603 | 0.578 |
| Layer 0 QKV | 1024 | 2.166 | 2.949 |
| Layer 0 FFN up | 1024 | 3.663 | 4.890 |
| Layer 0 FFN down | 1024 | 3.843 | 5.781 |

The SM89 compiler uses 76 registers, 29,184 bytes shared memory and
no spills. The candidate has two activation planes, two expanded
weight planes and a 64-token output tile, compared with the
control's 128-token tile. This trace does not separate occupancy,
staging and integer instruction bottlenecks.

All full-prefill geometries are slower. No HTTP dispatch was added
and no model-quality, prefill/decode, process VRAM or RAM claims
were made. EXP-2049 remains the working Bonsai HTTP reference. If
integer correction is revisited, it needs to avoid a second full
matrix product and include pack cost on the current baseline.
Apex and Inkling artifacts were untouched.

## Reproduce

```bash
exp=experiments/EXP-2026-11-25-2051-bonsai-mmq-int4-residual
out=build/bonsai-q8-q4-residual-2051
bash "$exp/source/build.sh" "$out" build/bonsai-flash-fused-padding-2049/pipeline.cl
CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration/inventory.json" "$out/pipeline.cubin"
```

[Component correctness, kernel times and hashes](data/component.json).
