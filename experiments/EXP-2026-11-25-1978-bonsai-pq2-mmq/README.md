# EXP-2026-11-25-1978 — Bonsai batched PQ2 tensor-core projections

**PROVEN at projection level.** The kernel stages one PQ2 block per weight row,
unpacks to FP16 in shared memory and reuses it across 32 tokens. Each warp
computes two m16n8k16 output fragments with FP32 accumulators. F32 inputs are
packed once into an FP16 buffer that sibling projections can share. The
model's weights stay in their original PQ2 representation in VRAM.

On RTX 4070, with real PQ2_0 weight tensors from the local
`Ternary-Bonsai-2.5-27B-PQ2_0-mtp.gguf` and a seeded input fixture,
the CUDA event bracket included input packing and projection in the
candidate arm. The control calls the existing FP32 matvec once per token.
The warmed sequence was A–B–B–A, independently for each projection.

| Projection | Tokens | FP32 matvec mean, ms | MMQ mean, ms | Relative L2 error |
| --- | ---: | ---: | ---: | ---: |
| First GDN QKV, 10239 of 10240 rows | 33 | 3.794 | 0.374 | 2.056e-4 |
| First GDN QKV | 128 | 15.360 | 0.745 | 2.063e-4 |
| First full-attention output | 128 | 9.322 | 0.494 | 2.084e-4 |
| First FFN down | 256 | 47.472 | 2.677 | 2.089e-4 |

Sampled all tokens and 12 output rows against independent FP64 dot products
with the same round-to-FP16 operand rule. Max error was 1.042e-4 and all
cases passed the `atol=2e-4, rtol=4e-5` limit. Full matrix comparison
against FP32 matvec passed the relative L2 limit of 5e-4. Odd token and
row counts exercise both output tails. The compiler used 40 registers
and 17,408 bytes shared memory per block with no spills.

These data do not measure graph throughput or numerical behavior after
64 layers. The FP16 activation conversion changes the precision contract;
long-prompt generation parity must be validated before accepting this as a
runtime default. The kernel is isolated from Apex and Inkling.

## Reproduction

From the repository root, using the EXP-1973 snapshot and local model.
The device module uses Clang 18 and ptxas targeting sm_89.

```sh
exp=experiments/EXP-2026-11-25-1978-bonsai-pq2-mmq
bash "$exp/source/build.sh" build/bonsai-mmq-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-mmq.py" \
  build/bonsai-integration build/bonsai-integration/inventory.json \
  build/bonsai-mmq-repro/mmq.cubin
```

[Raw timing and correctness results](results.json).
[PTX matrix fragment format](https://docs.nvidia.com/cuda/parallel-thread-execution/index.html#warp-level-matrix-fragment-mma-16816-float).
