# EXP-2026-11-24-1865 — Group two vocabulary projection rows in one CTA

**REJECTED at the standalone probe.** The earlier decode trace reports
456.26 ms in 511 dense MMVQ launches with grid X 248320. The Apex GGUF
header identifies `output.weight` as shape 2048 × 248320, Q6_K. The
probe halves the CTA count, retaining the original four warps,
accumulation order and independent shared scratch for each row. The
change does not alter the Q6 dequantizer or the sampler.

The control is the archived MMQ PTX on RTX 4070. Both images use 40
registers per thread and zero spills. Shared per CTA changes from 384
to 768 bytes. Synthetic Q6_K weights and Q8_1 activations cover the full
shape. All 248320 FP32 logits matched bit-for-bit. Eight alternating
blocks with 32 launches per arm yielded medians of 0.95839 and
0.95499 ms per launch, respectively. The 0.36% difference corresponds
to about 1.7 ms across 511 launches if it transfers. The standalone
control is not a full-model performance reference. No Apex candidate
was built or run.

Build by applying [cuda-mmvq.patch](cuda-mmvq.patch) to the frozen
EXP-1786 CUDA glue. Compile the frozen MMQ tree with Clang 18 flags
from [EXP-1859](../EXP-2026-11-24-1859-flash-interior/README.md) and
use CUDA 12.6 ptxas to extract `goose_mmvq_dense_matvec` for SM89
from each PTX. Use the archived PTX for the control.
Verify the cubins with [SHA256SUMS](SHA256SUMS), then run
`python3 probe.py` with NumPy. Raw CUDA-event times are in
[probe.log](probe.log) and [results.json](results.json).
