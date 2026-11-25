# EXP-2026-11-26-2087 — N64 tiles with paired PQ2 selectors

**Rejected at the component performance gate.** EXP-2086 stays
the cumulative checkpoint. No host routing or runtime changes.

## Mechanism

Use M64 × N64 rather than EXP-2086's M64 × N128 with the same
256-thread CTA. Each block consumes the first or second half of a
128-token Q8 panel. Preserve the existing pack, paired weight loads
and selectors, integer dot products and ordered FP32 accumulation.
The number of CTAs doubles for a full projection. The candidate
reduces registers from 79 to 55 and shared memory from 28,416 to
18,944 B per CTA. Neither kernel spills. The CUDA Driver occupancy
query reports a maximum of three versus four active blocks per SM.
This is a resource limit, not a measured runtime occupancy.

## RTX 4070 component probe

Same CUBIN and seeded activations, four real 5120-to-17408 PQ2_0
weights at 1024 tokens. Median of eight readings per arm in
balanced order, including the same activation pack. All bytes
match, the reference is finite, and the zero row is exact.

| Tensor | N128, ms | N64, ms | Elapsed increase |
| --- | ---: | ---: | ---: |
| Layer 0 gate | 1.73722 | 2.17974 | 25.47% |
| Layer 0 up | 1.73878 | 2.18317 | 25.56% |
| Layer 62 gate | 1.73810 | 2.17627 | 25.21% |
| Layer 62 up | 1.73584 | 2.32192 | 33.76% |

The higher residency limit does not compensate for the measured
cost of processing the smaller tile. The variant repeats weight
loading and expansion for both halves of each activation panel.
The experiment does not isolate the cost of those repeats from
the other consequences of the geometry change.

The candidate misses the 5% gate. Full-request speed and process
memory are unmeasured. The retained EXP-2086 result remains
1015.48 prefill / 32.59 decode tok/s and 8,932 MiB process VRAM.
Saved PrismML is 1076.60 / 47.63 tok/s on the same 32K/512 workload.

## Next probe

Investigate the full-tile FP16 half2 FFN down path. It still uses
about 5.3 s of prefill GPU time. EXP-2085 ruled out changing only
its activation layout at the 5% gate. A different hypothesis is
to reduce the repeated scale and code reads when staging the PQ2
block for two consecutive K steps. Keep the existing FP16 operand
bits and ordered MMA boundaries when testing a staging change.

## Reproduce

Use the EXP-2086 build directory with its OpenCL and combined PTX.
Build with Clang 18 and CUDA ptxas for SM89. The harness uses the
EXP-1973 bootstrap directory and GGUF inventory.

```bash
exp=experiments/EXP-2026-11-26-2087-bonsai-q8-spread-n64
out=build/bonsai-q8-spread-n64-2087
check=experiments/EXP-2026-11-26-2081-bonsai-q8-paired-weight-load/source/check.py
bash "$exp/source/build.sh" "$out" build/bonsai-q8-paired-selector-2086
OPENBLAS_NUM_THREADS=1 python3 "$check" CORE_DIRECTORY INVENTORY "$out/pipeline.cubin" spread spread_n64
```

[Component timings](data/component.json), [CUDA occupancy](data/occupancy.log),
and [build receipt](data/build.json).
