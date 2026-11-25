# EXP-2026-11-25-1993 — Bound the cost of PQ2 matrix preparation

**REJECTED for runtime promotion.** This experiment compared two ways to
move PQ2 preparation out of the repeated wide MMQ tile. The reference
is EXP-1992's asynchronous 64x64 matrix kernel.

- FP16 expansion materializes one dense matrix into reusable GPU scratch.
  Both operands then reach shared memory with 16-byte asynchronous
  copies. The graph still uses the original PQ2 storage.
- Compact planes transpose quant blocks by K block and matrix row and
  separate 32-byte code rows from the two-byte scales. The MMQ kernel
  reads these aligned planes and expands the same four weights per code
  byte in shared memory. Scratch stores 34 bytes per PQ2 block.

All three modes share the same half activation pack and ordered FP32 MMA
sequence. No runtime path was switched because neither layout showed a
sufficient component gain after preparation.

## Component A/B

RTX 4070, same real GGUF matrices and seed as EXP-1992. Prepacked
activations are common. CUDA events include preparation and the matrix
launch for each candidate. Medians of four samples. All output element
bytes match on all six shapes, including the narrow/row-tail case.

| Matrix / tokens | Resident PQ2, ms | FP16 scratch, ms | Compact planes, ms |
| --- | ---: | ---: | ---: |
| QKV tail / 33 | 0.281 | 0.536 | 0.280 |
| QKV / 257 | 1.101 | 1.497 | 1.079 |
| QKV / 1024 | 3.243 | 4.137 | 3.177 |
| Attention output / 1024 | 1.981 | 2.365 | 1.935 |
| FFN up / 1024 | 5.494 | 6.988 | 5.432 |
| FFN down / 1024 | 5.589 | 6.160 | 5.531 |

Full FP16 expansion alone took up to approximately 0.58 ms; the maximum
scratch on these fixtures was 170 MiB. Excluding expansion, the expanded
FFN up kernel remains slower and FFN down approaches the control. Enlarged
weight traffic is a plausible contributor. No hardware counter
attribution was made. Changing to compact planes costs at most 22.58 MiB
of scratch here and yields about 0.8–2.4% elapsed time reduction for the
1024-token fixtures including preparation. The retained source keeps
the option of testing an aligned permanent layout later if profiling
justifies its complexity. This experiment did not benchmark the
layout as an on-load repack.

Component rejection ends this design before model A/B. No claim about
generation or full request timing was made. Shared Apex, Inkling and
Bonsai runtime execution paths remain unchanged. The next larger MMQ
lever is integer Tensor Core accumulation inside PQ2 scale blocks
with quantized activations. Since that changes numeric precision, it
requires an independent component error comparison and short greedy
ID tests before it can be promoted.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-1993-bonsai-mmq-expand
bash "$exp/source/build.sh" build/bonsai-mmq-expand-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-expand.py" \
  build/bonsai-integration build/bonsai-integration/inventory.json \
  build/bonsai-mmq-expand-repro/pq2-expanded.cubin
```

[Component data and scratch sizes](component.json).
