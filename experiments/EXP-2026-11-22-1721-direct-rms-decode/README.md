# EXP-1721 — Specialize single-row decode RMSNorm

A contiguous decode row of width 2048 uses one float4 per lane and
the existing FP32 square accumulation and reduction tree. The
launch passes pointers and epsilon directly. The dedicated kernel
avoids the generic tensor descriptor and coordinate arithmetic.
Its sm_89 entry uses 20 registers, 128 bytes of shared memory, and
no spills. Other shapes, prefill, and RMS with postscale use the
original entry. The candidate log confirms use of the new path.

## Full warmed Apex pair

RTX 4070, Apex Q4_K_M, 32,768 input and 512 output tokens,
batch/ubatch 4,096/4,096, Q4_0 KV, context 262,144,
6,290 MiB expert bank, 210 MiB temporary workspace, no probability
rows. Both unprofiled arms ran one warm-up and one measured
request, with zero prompt reuse.

| Metric | Fresh EXP-1720 | EXP-1721 |
|---|---:|---:|
| Prefill, tok/s | 3017.68 | 3015.02 |
| Decode, tok/s | 48.74 | 51.09 |
| Decode, ms | 10484.757 | 10001.393 |
| Client, s | 21.433457 | 20.962267 |
| Peak process VRAM, MiB | 8766 | 8766 |
| Peak RSS, KiB | 21487448 | 21586940 |
| Process swap, KiB | 0 | 0 |
| System swap-out since launch, pages | 47 | 0 |

The measured pair saves 483.364 ms decode and 471.190 ms client
time. The fresh control is slower than EXP-1720's historical
49.95 tok/s decode. The new 51.09 measure is also above that
historical figure. Decode in the warm-up is 50.44 versus 50.79
tok/s. The reduced GPU-kernel time has not been profiled, so the
full paired delta is not all attributed to the RMS operation.

Both requests match all 512 token IDs, text and SLRU counts. The
candidate records 97.16 MiB higher peak RSS. Keep it as an
experimental checkpoint with EXP-1720 for rollback.

Exact inputs, device sources, binaries, and raw results are under
`build/exp1721-direct-rms-decode`. The candidate binary SHA-256 is
`54199b1b7f24b425ecf024120f52a573138d7a6a5e96dc6d140dbca34505bddd`.
Apply the supplied patches to the EXP-1720 driver and OpenCL glue
and the EXP-1665 extracted RMS fragment. Rebuild both CUDA images
and replace the driver object in the EXP-1720 archive. Check that
the new PTX is embedded in the executable before the pair.
