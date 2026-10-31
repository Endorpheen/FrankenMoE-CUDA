# EXP-2026-10-31-134: restricted Goose-to-C-to-CUDA compilation

Status: FEASIBILITY_CONFIRMED (no baseline promotion).
User-requested language/interop experiment; accepted EXP107 left unchanged.

Pure scalar SiLU-gate written in Goose, emitted as C by pinned v2026.3,
restricted device adaptation and NVCC sm89 compilation. Separate AOT Goose
host loop calls the same C ABI CUDA bridge as C++. No full-language CUDA
backend, framework rewrite or host-wrapper-only kernel claim.

Six sizes1..4194304, including tails and special floats: byte-identical GPU
outputs. Memcheck0. Both kernels have identical160-instruction disassembly.
Five alternating pairs per timing case,40 serial timing blocks total. Empty
launch enqueue medians: C++1.66036us, Goose1.62352us; about-37ns observed
separation, insufficient to establish a systematic penalty. Large-kernel
stream-event intervals109.117/124.590us; no speedup claimed with identical code.

No model benchmark, production memory claim or CUDA Graphs integration. Model
server remained idle; no installed settings/source/binary changed. EXP133 CPU
profiling remains separately prepared and unmeasured until this report closes.

[Sources, actual transformation, timings and limitations](../results/exp134-goose-cuda/README.md).
