# EXP-2026-11-26-2083 — M128 × N128 Bonsai INT8 gate/up

**Rejected at the component performance gate.** EXP-2081 remains
the cumulative checkpoint. No host routing or runtime changes.

## Mechanism

EXP-2082 found 8.482 s of gate/up GPU time across 3,968 calls.
This probe reuses EXP-2081's exact paired PQ2 unpack and Q8 pack.
Double the row tile to 128 and use 16 warps per CTA so an activation
panel feeds twice as many weight rows. The number of CTAs for a
full 5120-to-17408, 1024-token projection decreases from 2176 to
1088. Each output keeps the same integer MMA and FP32 accumulation
order. Shared memory increases from 28,416 to 37,888 B. SM89 uses
92 versus 93 registers per thread, with zero spills in both cases.
Register allocation permits at most two 256-thread control CTAs
versus one 512-thread candidate CTA per SM. Both counts correspond
to 16 warps, assuming enough work and no other occupancy limit.
The timing does not isolate barrier cost or per-warp stall reasons.

## RTX 4070 component probe

Same CUBIN and seeded activations, four real 5120-to-17408 PQ2_0
weights, 1024 tokens. Median of eight timings per arm in balanced
order, with activation packing included. All outputs match bitwise
and the reference is finite with an exact zero row.

| Tensor | M64 × N128, ms | M128 × N128, ms | Elapsed increase |
| --- | ---: | ---: | ---: |
| Layer 0 gate | 2.1745 | 2.3981 | 10.29% |
| Layer 0 up | 2.1960 | 2.4013 | 9.35% |
| Layer 62 gate | 2.1816 | 2.4013 | 10.07% |
| Layer 62 up | 2.1786 | 2.4145 | 10.83% |

The result misses the preregistered 5% time reduction. Full-request
speeds and process memory for M128 are unmeasured. The unchanged
EXP-2081 scored reference is 976.17 prefill / 32.65 decode tok/s
with 8,932 MiB process VRAM after request. Saved PrismML is
1076.60 / 47.63 tok/s on the same 32K/512 workload.

## Follow-up hypothesis

Preserve M64 × N128 and investigate block-major Q8 pack layout.
The existing pack groups by token, so consecutive activation rows
within the same K block are separated by 5120 B. A block-major
layout would make these rows adjacent for the cooperative copies.
It needs a matching writer and consumer, with identical byte and
scale values, the same storage size, and timing that includes
pack. Probe this independently of CTA widening.

## Reproduce

Use the EXP-2081 build directory, including its `component.cl`.
Build using Clang 18 and CUDA ptxas for SM89. The component harness
takes the EXP-1973 bootstrap directory and GGUF inventory.

```bash
exp=experiments/EXP-2026-11-26-2083-bonsai-q8-m128n128
check=experiments/EXP-2026-11-26-2081-bonsai-q8-paired-weight-load/source/check.py
build=build/bonsai-q8-m128n128-2083
bash "$exp/source/build.sh" "$build" build/bonsai-q8-paired-weight-load-2081
OPENBLAS_NUM_THREADS=1 python3 "$check" CORE_DIRECTORY INVENTORY "$build/pipeline.cubin" paired m128
```

[Component results](data/component.json) and [build receipt](data/build.json).
