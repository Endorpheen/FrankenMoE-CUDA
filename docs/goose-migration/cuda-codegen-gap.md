# The CUDA items share one cause: a codegen gap, measured twice

Two CUDA transfers are functionally correct and were rejected on the same ground — the Goose-generated
kernel costs more than the hand-written one:

| transfer | verdict | measured cost |
|---|---|---|
| Q6_K matvec (EXP186) | REJECTED, opt-in | −3.66 % decode; kernel 21.82 µs vs 17.41 µs; SASS 504 vs 193 instructions |
| MMQ Q8_1 quantizer (EXP180) | REJECTED, opt-in | kernel 2065 vs 1819 ns median (−12–14 % per launch); paired Apex showed the regression |

Both are byte-exact and both reproduce the Apex answer `e68df05b`. So this is not a porting error: the
emitted code is correct and slower.

## What that means for the plan

APEX.CUDA_MATVEC and APEX.CUDA_MMQ cannot be closed by another correct transfer. Their remainder is
performance, and performance here is decided by the compiler's code generation for CUDA kernels, not by the
runtime worker's transcription. Two concrete asks were sent to the compiler side:

1. the SASS gap on the Q6_K matvec (504 against 193 instructions) — the transfer's structure matches
   EXP185's Q4_K, which was accepted, so the difference is in what the backend emits;
2. the Q8_1 quantizer's per-launch overhead (2065 against 1819 ns) — likely address arithmetic and
   redundant loads in the emitted kernel.

Until those are addressed, promoting either transfer would trade correctness-neutral speed for the word
"Goose-owned", which the acceptance rules forbid.

## What is already Goose-owned here

Q4_K matvec (EXP185, accepted with a neutral A/B), FWHT (EXP183), mmvf (EXP184), mm ids (EXP181), dequant
(EXP182, EXP188), and the MMQ dispatch decision (FG203D, live). cuBLAS/CUTLASS remain the declared
primitives, as the criterion allows.
