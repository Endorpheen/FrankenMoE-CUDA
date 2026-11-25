# EXP-2026-11-26-2090 — Joint K128 staging for Bonsai FFN down

**Rejected at the component performance gate.** EXP-2086 remains the
cumulative checkpoint. No host routing, runtime or checkpoint change.

## Mechanism

The active half2 FFN down kernel stages two K64 steps per PQ2 block
and uses a copy wait and two CTA barriers per step. The candidate
loads both activation halves and expands both weight halves before
one copy wait and one pair of barriers per K128 block. It uses
the same full M64 × N128 geometry and FP16/FP32 accumulation sequence,
including both K32 partial boundaries in each half. The fused
SwiGLU/FWHT/FP16 producer is identical. The CUDA image adds a
separate test function.

Per-CTA shared memory doubles from 27,648 to 55,296 B. The larger
entry uses opt-in dynamic shared memory. Both entries use 79
registers per thread and zero spills. The CUDA Driver's occupancy
query gives three versus one maximum active CTAs per SM at 256
threads. This is a resource limit rather than achieved occupancy.

## RTX 4070 component probe

Same CUBIN and seeded activation inputs. Four real 17408-to-5120
PQ2_0 down matrices, 1024 tokens, real Hadamard signs. Eight
observations per arm in balanced order, including the fused
activation producer. All outputs are finite and bitwise identical,
with an exact zero-input row.

| Tensor | Separate K64 steps, ms | Joint K128 staging, ms | Elapsed increase |
| --- | ---: | ---: | ---: |
| Layer 0 down | 3.11859 | 4.57434 | 46.68% |
| Layer 21 down | 3.08075 | 4.31360 | 40.02% |
| Layer 42 down | 3.15462 | 4.43749 | 40.67% |
| Layer 63 down | 2.97594 | 4.18202 | 40.53% |

The result misses the preregistered 5% time reduction. The candidate
reduces barriers but also cuts maximum block residency, so this test
cannot isolate barrier savings. No full workload or process memory
measurements were run for the candidate. The retained EXP-2086 mean
remains 1015.48 prefill / 32.59 decode tok/s and 8,932 MiB process VRAM.
Saved PrismML is 1076.60 / 47.63 tok/s on the same 32K/512 workload.

## Follow-up

Use the saved PrismML execution trace and exact compiled source to
check which PQ2 matrix path runs on SM89. The SM90+ specialization
in the source alone is not evidence that it ran on this RTX 4070.
Compare the executed projection geometries and pipeline behavior
with EXP-2086 before trying another isolated tile change. Use CPU
sampling to attribute host gaps where the measured wall time warrants it.

## Reproduce

Use the EXP-2086 combined PTX and EXP-2071 OpenCL source. Compile with
Clang 18 and CUDA ptxas for SM89. The harness requires the EXP-1973
bootstrap directory and GGUF inventory.

```bash
exp=experiments/EXP-2026-11-26-2090-bonsai-down-k128
out=build/bonsai-down-k128-2090
check=experiments/EXP-2026-11-26-2085-bonsai-ffn-down-panel/source/check.py
bash "$exp/source/build.sh" "$out" build/bonsai-q8-paired-selector-2086 build/bonsai-cumulative-half-unpack-2071/pipeline.cl
OPENBLAS_NUM_THREADS=1 python3 "$check" CORE_DIRECTORY INVENTORY "$out/pipeline.cubin" k128
```

[Component timings](data/component.json), [CUDA occupancy query](data/occupancy.log),
and [build receipt](data/build.json).
