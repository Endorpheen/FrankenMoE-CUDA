# EXP-1713 — Share packed K/V across four query heads

The long packed Q4_0 path shares key loads and value dequantization
across four query heads of one KV head. Each query keeps separate
FP32 dot products, softmax state, and output partials. The partition
boundaries, per-head reduction order, and combine kernel are preserved.
The driver retains the two-head and single-head entries as fallbacks.

For sm_89, the four-head entry uses 132 registers per thread and
4,608 bytes of shared memory, with zero spills. The previous entry uses
94 registers and the same shared memory. The candidate server log
confirms that Apex selected the four-head entry.

## Full warmed Apex pair

Apex-35B-V1.0 Q4_K_M on RTX 4070, 32,768 input and 512 output tokens,
batch/ubatch 4,096/4,096, context 262,144, Q4_0 KV, 6,290 MiB expert
bank, 210 MiB temporary prefill workspace, default API without
probability reporting. Both arms used the EXP-1712 profile and ran
one full warm-up in the same process. Prompt reuse was zero. No
profiler was enabled.

| Metric | EXP-1712 fresh control | EXP-1713 |
|---|---:|---:|
| Prefill, tok/s | 2995.89 | 3002.35 |
| Decode, tok/s | 43.65 | 45.49 |
| Decode, ms | 11707.91 | 11234.20 |
| Client, s | 22.734605 | 22.240415 |
| Peak process VRAM, MiB | 8766 | 8766 |
| Peak RSS, KiB | 21490904 | 21585428 |
| Process swap, KiB | 0 | 0 |
| System swap-out, pages | 0 | 0 |

Decode saves 473.714 ms and the client saves 494.190 ms. All 512 IDs,
response text and SLRU counters match, including both warm-up and
measured outputs. The candidate uses 92.31 MiB more peak RSS in this
pair. The same two-head control binary previously peaked at 21,584,244
KiB, within 1.16 MiB of the four-head result.

Keep EXP-1713 as the fastest measured experimental checkpoint.
EXP-1712 and EXP-1710 are available for rollback. The full-profile
target remains 3043 prefill and 71.17 decode tok/s.

## Reproduction

Executable, archive, profile, CUDA images, and raw results are in
`build/exp1713-flash-gqa-quad`. Candidate binary SHA-256 is
`4f4a1084f6d8ca71bd4e703907806a8b6a14da0045a12735b2824e7e022cca29`.
Apply these patches to the EXP-1712 driver and device sources,
using the corrected EXP-1712 OpenCL patch. That correction reproduces
the original EXP-1712 sm_89 MMQ image byte for byte
(SHA-256 `82ed7388ad826d02f1e1e63a846df13f83b85996ffa4dc315b7e5b59a692d81d`).
Regenerate the CUDA images and replace only the driver object in
the EXP-1712 archive.
