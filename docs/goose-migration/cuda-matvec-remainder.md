# APEX.CUDA_MATVEC: the exact remaining blocker, measured

The item asks that the used dense/quantized matvec branches be Goose-owned, with cuBLAS/CUTLASS as
explicit primitives, and names the Q6_K path as its remainder. That remainder was implemented, and it was
**rejected on measurement**, not on a hunch:

| arm (same DSO, only this op switched) | prefill tok/s | decode tok/s |
|---|---:|---:|
| native-rollback a | 2561.06 | 26.52 |
| goose a | 2582.11 | 25.95 |
| goose b | 2579.59 | 25.87 |
| native-rollback b | 2603.62 | 27.26 |
| native median | 2582.34 | 26.89 |

The transfer is byte-exact (4 of 4 outputs identical across ncols 256/1024/2048/4096, memcheck clean, the
model witness present, Apex content `e68df05b`), but it costs **−3.66 % decode**, from a measured kernel
cost of 21.82 µs against 17.41 µs (SASS 504 against 193 instructions).

## Consequence

The Q6_K branch stays opt-in (`GGML_GOOSE_CUDA_MMVQ_Q6K=1`) and out of the active chain. Closing the item
therefore needs one of:

1. a cheaper Goose Q6_K kernel (the codegen gap is the cause, not the algorithm), or
2. an explicit acceptance decision that the transfer is worth −3.66 % decode, which is not mine to make.

Everything else in the item is already Goose-owned or declared primitive: Q4_K matvec (EXP185, accepted),
FWHT (EXP183), mmvf (EXP184), mm_ids (EXP181), dequant (EXP182/188), MMQ dispatch (FG203D).
'''
git add docs/goose-migration/cuda-matvec-remainder.md && git commit -q -F - <<'EOF'
docs: CUDA_MATVEC's remainder is a measured codegen gap, not missing work #insight

The Q6_K matvec was transferred and is byte-exact, but it costs -3.66 % decode (21.82 vs 17.41 us per
kernel, SASS 504 vs 193), so it stays opt-in. Closing the item needs either a cheaper Goose kernel or an
explicit acceptance of that cost, which is not mine to decide.

Recorded with the paired A/B arms so the reason can be checked rather than remembered.

Co-authored-by: CommandCodeBot <noreply@commandcode.ai>
