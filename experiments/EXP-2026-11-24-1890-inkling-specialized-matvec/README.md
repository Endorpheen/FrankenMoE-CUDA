# EXP-2026-11-24-1890 — Specialize Inkling quantized projection batches

**Accepted diagnostic candidate.** Replace Inkling's generic CUDA matvec
dispatch for Q8_0, IQ4_NL and IQ4_XS with standalone Goose kernels and
SM89 glue. Four independent row warps share a CTA. A packed table decodes
the IQ4 nibbles. On batches the warp can reuse one decoded weight across
four jobs when their expert-plane offsets agree. Small or mixed-plane
groups use the one-job path. Every output retains its reference FP32
FMA sequence and ordered warp butterfly. The host bridge only selects
these entries when the existing Inkling quantization gate is enabled.

RTX 4070, Inkling AD-IQ4_XS, context 128, EXP-1889 input (24 IDs),
microbatch 16 plus tail eight, 17 greedy outputs, 16 decode intervals.
Same binary, EXP-1888 device Attention and two-plane expert bank.
Control uses `I2_SPECIALIZED_MATVEC=0`, candidate uses `1`.

| Arm | Prefill seconds | Decode tok/s |
| --- | ---: | ---: |
| A1 | 5.784 | 3.237 |
| B1 | 3.512 | 5.031 |
| B2 | 3.467 | 5.056 |
| A2 | 5.825 | 3.205 |

All 17 output IDs, 57 residual/logit fingerprints, SLRU stats and
Attention transfer counters match. Peak RSS around 8.75 GiB,
process GPU allocation around 7.16 GiB and swap-out zero. Both
pairs improved in both phases. This accepts the change for the
short diagnostic line. Full-context model and HTTP qualification remain open.

Separate Nsight candidate and generic traces show quantized matvec
falling from 2587 to 291 ms in prefill and 2106 to 341 ms in decode
(includes the unchanged generic F32 router). In that decode phase,
24848 context synchronizations, 25616 D2H copies and 35264 H2D copies
remain. The next large opportunity is keeping activations resident
across an entire visit and using graph replay with the SLRU and
quantized kernels, not another isolated math tweak.
Apex EXP-1880 is the independent regression control.

## Reproduce

Compile `source/inkling-matrix.goose` using standalone KeelWing provenance
and the EXP-1786 fragment adapter with module name `inkling-matrix`.
Use `source/matrix.cl`, the EXP-1786 device prelude, Clang 18 SM80 NVPTX
with `-O3 -ffp-contract=off`, and CUDA 12.6 SM89 ptxas. Embed the resulting
cubin via `xxd -i matrix.cubin` as `matrix-image.h`. Apply
`source/driver.patch` to the EXP-1888 historical driver included by
the Inkling adapter, then rebuild its archive member with GCC 13 `-O1`.
Use the EXP-1889 harness and flags, adding the control/candidate switch
above. `results.json` and `trace-results.json` contain the
measurement receipts. `README.md`, source and receipts are hashed.
