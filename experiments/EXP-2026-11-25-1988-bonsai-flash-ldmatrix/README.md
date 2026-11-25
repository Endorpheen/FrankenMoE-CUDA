# EXP-2026-11-25-1988 — Bonsai Flash async staging and warp fragment loads

**PROVEN for component performance and exact short output.** Experimental
mode pending full throughput qualification. The control is compact
staged FP32 Flash from EXP-1987. Candidate B replaces scalar fragment
loads with `ldmatrix.x4/x2` over aligned 8x8 halfword submatrices.
Candidate C also transfers K and V in 16-byte `cp.async` parcels from
the packed global staging area directly into shared memory. Each thread
waits for its copies before the CTA synchronization. Out-of-range
packed chunks zero-fill the operand. The placement of actual elements,
causal mask, FP32 accumulation, and ordered merge stay unchanged.

The component test uses the same fixture and GPU event scope as EXP-1987.
The order is A–B–C–C–B–A. Times include K/V staging, tile and merge.
All three outputs match byte for byte on all five cases. Relative L2
against frozen Q4 tiles remains at most 2.95e-4.

| KV rows / queries | Scalar loads, ms | ldmatrix, ms | ldmatrix + cp.async, ms |
| --- | ---: | ---: | ---: |
| 65 / 9 | 0.0348 | 0.0306 | 0.0241 |
| 257 / 33 | 0.0569 | 0.0528 | 0.0379 |
| 8192 / 128 | 4.620 | 4.559 | 2.027 |
| 32768 / 128 | 17.750 | 17.533 | 8.308 |
| 1024 / 1024 | 1.713 | 1.635 | 0.954 |

The combined kernel cuts elapsed time by 53.2% at 32768/128. Relative
to the historical Q4 component from EXP-1982 at that shape (220.944 ms),
it is about 26.6x. The compiled tile register counts are 93, 72 and 64
respectively, with no spills. All three reserve 41,216 bytes of shared
memory per CTA. The earlier 3.63 ms approximate Flash result still
serves as a component speed marker with a failed accuracy gate.

## Short model gate

RTX 4070, 4097/16, context 36864, ubatch 1024, Q4_0 K/V, raw input IDs
from EXP-1977, greedy with EOS suppressed and MTP disabled. A–B–B–A on
one binary, where A uses scalar movement and B uses async movement
with ldmatrix. All four Goose outputs match byte for byte. All 16
IDs match the saved same-input PrismML result in EXP-1985. The inherited
ordered matvec is active in both modes.

Prefill saves 71.389 and 114.910 ms in the two pairs. Decode spent
29.058 and 59.768 ms more in those pairs. The candidate does not
switch any decode kernel. The data do not establish the cause
of the timing difference. Both modes allocate exactly 1,720,400,640 B
GPU workspace. Peak RAM and VRAM were not sampled. Preserve the
reference path until the performance gate is fully qualified. No
32K/512 full model request ran as part of this experiment.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-1988-bonsai-flash-ldmatrix
bash "$exp/source/build.sh" build/bonsai-flash-async-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-ldmatrix.py" \
  build/bonsai-integration build/bonsai-integration/inventory.json \
  build/bonsai-flash-async-repro/flash-ldmatrix.cubin \
  build/bonsai-flash-async-repro/frozen-goose.ptx
```

Use the EXP-1987 4097/16 invocation with `bonsai-flash-async` and
`flash-ldmatrix.cubin`. Enable the candidate with `BONSAI_FLASH_ASYNC=1`.
Shared Apex and Inkling source and device images are unchanged.

[Component data](component.json). [Short model data](results.json).
[PTX matrix loads and async copies](https://docs.nvidia.com/cuda/parallel-thread-execution/index.html).
