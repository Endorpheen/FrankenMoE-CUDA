# EXP-1710 — Overlap expert down transfers and specialize short decode matvecs

The existing expert split flag did not split the queued transfers on Apex.
The first SLRU lookup had already staged all three planes when the old
driver saved the boundary. The new driver identifies the trailing down
plane copies in the actual jobs and verifies that the weight spans do
not alias. The normal completion event continues to protect the down
projection. No SLRU policy or transfer order changes.

The short MMVQ specialization handles four rows per 128-thread block
on the routed down projection for widths of 256 or 512. Each row uses
the first-warp reduction from the original kernel. The old kernel is
retained for other shapes. EXP-1706 matched 65,536 FP32 test outputs and
the live 4,096/32 trace measured 44.268 to 31.570 ms over 1,810 down
projections. EXP-1708 increased actual copy/kernel overlap on its
matching 4,096/32 trace. Neither short measurement is used as the
32,768/512 throughput result.

## Full warmed pair

Apex-35B-V1.0 Q4_K_M, RTX 4070, 32,768 input and 512 output tokens,
batch/ubatch 4,096/4,096, 6,290 MiB expert cache, 210 MiB temporary
prefill workspace, 18,601 MiB requested host pin, Q4_0 KV, context
262,144, default API without probability rows. Each binary ran one
full warm-up request in the same process before its measured request.
Both measured requests had zero prompt reuse. No profiler ran.

| Metric | Fresh EXP-1701 control | EXP-1710 |
|---|---:|---:|
| Prefill, tok/s | 3006.25 | 3000.79 |
| Decode, tok/s | 40.11 | 41.63 |
| Decode, ms | 12739.26 | 12275.27 |
| Total client time, s | 23.729007 | 23.288728 |
| Peak process VRAM, MiB | 8766 | 8766 |
| Peak RSS, KiB | 21488556 | 21490236 |
| Process swap, KiB | 0 | 0 |
| System swap-out, pages | 0 | 0 |

The candidate saves 463.998 ms decode and 440.279 ms client time.
Prefill takes 19.805 ms longer in this pair. Both warm-ups and both
measured requests matched all 512 token IDs and all response bytes.
All SLRU counters match. Content SHA-256 is
`f8664331bd30cac8124f48807211ec41b6d0b2b77e301839d8a605e8e2a6e4ce`.

Adopt EXP-1710 as the decode checkpoint. Preserve EXP-1701 as the
rollback point and historical prefill best (3035.23 tok/s at
41.06 tok/s decode). The historical measurement is separate from
the fresh control above.

## Reproduction

The local `build/exp1710-decode-split-and-down` directory contains
`apex-pair.sh`, the executable, archive, full raw pair, and
`paired-full-results.txt`. Candidate binary SHA-256 is
`5db197c7cb82a3815e7e9c91d4e5dfaed2bb343ace4d10eefc31644c1d60b094`.
The pair script selects `GOOSE_CUDA_EXPERT_SPLIT_PLANES=1` only
for the candidate. Both arms use the EXP-1701 request and profile.

Apply the supplied driver patch to the cumulative EXP-1694
`cuda-driver.c`, the device Goose patch to the EXP-1665 extracted
`mmvq.goose`, and the OpenCL glue patch to the repository
`integration/goose-standalone/cuda-mmvq.cl`. Rebuild the CUDA images
and replace only `cuda-driver.o` in the EXP-1701 archive, then relink.
