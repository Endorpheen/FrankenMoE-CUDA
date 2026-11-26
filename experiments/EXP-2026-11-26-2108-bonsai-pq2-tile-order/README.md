# EXP-2026-11-26-2108 — Group Bonsai gate/up tiles by row

Rejected at the component gate. The EXP-2106 trace saved with EXP-2107
assigns 6.827 s to the PQ2 INT8 gate/up kernel. The baseline orders the 272
row tiles before advancing to the next of eight token tiles.
Group two, four or eight token tiles for a row tile to test whether
shorter spacing between accesses to its packed weights reduces
elapsed time.

The variants use a bijective remapping of the existing 272 × 8 grid,
with 256 threads per CTA. They preserve the activation pack, PQ2
weights, shared layouts, MMA operations and FP32 accumulation order.
The benchmark runs the active EXP-2107 CUBIN as control, reuses its
activation pack across all arms and tests four real GGUF projections
at width 5120, 17408 output rows and 1024 tokens. All 17,825,792
FP32 outputs per projection and variant match the control bitwise.

## RTX 4070 kernel medians

Balanced order with 12 CUDA-event samples per variant and projection.
Times exclude the common activation pack and transfers.

| Projection | Baseline, ms | Group 2, ms | Group 4, ms | Group 8, ms |
| --- | ---: | ---: | ---: | ---: |
| Layer 0 gate | 1.649 | 1.732 | 2.184 | 2.092 |
| Layer 0 up | 1.551 | 1.638 | 2.075 | 1.990 |
| Layer 62 gate | 1.546 | 1.643 | 2.059 | 1.981 |
| Layer 62 up | 1.484 | 1.595 | 1.982 | 1.901 |

Group 2 is 5.03–7.46% slower. Group 4 is 32.45–33.74% slower
and group 8 is 26.89–28.26% slower. The variants use 80, 94 and 94
registers with 28416 bytes shared memory and zero spills. The
component does not resolve cache traffic or the source of the
slowdown. Keep the existing row-first schedule. Reconsider tile
grouping if the weight or packed activation layout changes.

No runtime or model server changes. The accepted EXP-2107 MTP
checkpoint stays at 1016.79 prefill / 72.33 effective decode tok/s
with 9438 MiB process VRAM after the request. The next distinct
opportunity in the same trace is the FP16 5120-wide PQ2 family,
which accounts for 4.982 s across 4464 calls.

## Reproduce

Use the archived EXP-2086 component OpenCL source and the pinned
EXP-2107 target CUBIN. Set `FRANKENMOE_CHECKPOINTS` to the archived
input directory with the Bonsai inventory and core.

```bash
exp=experiments/EXP-2026-11-26-2108-bonsai-pq2-tile-order
out=build/bonsai-pq2-tile-order-2108
bash "$exp/source/build.sh" build/bonsai-q8-paired-selector-2086/component.cl "$out"
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
    "$FRANKENMOE_CHECKPOINTS/bonsai-integration" \
    "$FRANKENMOE_CHECKPOINTS/bonsai-integration/inventory.json" \
    build/bonsai-mtp-draft-q4-2107/pipeline.cubin "$out/order.cubin"
```

[Raw component timings and hashes](data/component.json).
