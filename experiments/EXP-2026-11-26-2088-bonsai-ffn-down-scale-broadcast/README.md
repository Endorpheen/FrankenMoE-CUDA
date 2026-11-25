# EXP-2026-11-26-2088 — Warp scale broadcast for Bonsai FFN down

**Rejected at the component performance gate.** EXP-2086 stays
the cumulative checkpoint. The probe adds an isolated CUDA entry
and has no host dispatch changes.

## Mechanism

Target the current 17408-to-5120 full half2 FFN down prefill kernel.
Sixteen consecutive lanes expand weights for one output row at a
time. At the even K step they each read the same 16-bit PQ2 scale
and load distinct low and high code bytes for both steps. Let one
lane per row read the scale and broadcast its original bits with
a warp shuffle. Keep code reads, fused SwiGLU/FWHT/FP16 preparation,
expanded FP16 operands, shared geometry and ordered MMA unchanged.
At SM89 the candidate uses 80 registers rather than 79, 27,648 B
shared memory, and no spills. It adds no allocation.

## RTX 4070 component probe

Same CUBIN and seeded activations, four real PQ2_0 down projections
at 1024 tokens. Eight timings per arm in balanced order, including
the original fused activation producer. All outputs were finite and
bitwise identical. The zero-input row was exact.

| Tensor | Shared-address load, ms | Warp broadcast, ms | Elapsed increase |
| --- | ---: | ---: | ---: |
| Layer 0 down | 3.08221 | 3.12770 | 1.48% |
| Layer 21 down | 3.09603 | 3.12661 | 0.99% |
| Layer 42 down | 3.08461 | 3.12821 | 1.41% |
| Layer 63 down | 3.08517 | 3.12976 | 1.45% |

The result misses the 5% selection threshold. The experiment
compares complete components. It does not measure scale transactions
or prove the reason for the slowdown. Identical addresses can be
served together in the control, and the added shuffle introduces a
dependency. The recorded full-workload EXP-2086 reference remains
1015.48 prefill / 32.59 decode tok/s and 8,932 MiB process VRAM.
Saved PrismML is 1076.60 / 47.63 tok/s. Full-request speed and process
memory for this variant are unmeasured.

## Next probe

Separate the active half2 kernel's staging, unpack, synchronization
and MMA costs before selecting another change. Adapt EXP-1995's
per-CTA partitioning to the current full FFN down entry rather than
reusing numbers from the earlier 64 × 64 path. Measure profiler
overhead separately. Use the resulting attribution to prioritize a
new mechanism and score it first on the identical component fixture.

## Reproduce

Use the EXP-2086 combined PTX and EXP-2071 OpenCL source. Compile with
Clang 18 and CUDA ptxas for SM89. The component harness takes the
EXP-1973 bootstrap directory and GGUF inventory.

```bash
exp=experiments/EXP-2026-11-26-2088-bonsai-ffn-down-scale-broadcast
out=build/bonsai-ffn-down-scale-broadcast-2088
check=experiments/EXP-2026-11-26-2085-bonsai-ffn-down-panel/source/check.py
bash "$exp/source/build.sh" "$out" build/bonsai-q8-paired-selector-2086 build/bonsai-cumulative-half-unpack-2071/pipeline.cl
OPENBLAS_NUM_THREADS=1 python3 "$check" CORE_DIRECTORY INVENTORY "$out/pipeline.cubin" broadcast
```

[Component observations](data/component.json) and [build receipt](data/build.json).
