# EXP-1717 — Fuse the shared expert scale and residual additions

The grouped MoE graph scales the shared FFN output, adds it to the
routed FFN output, and adds the residual. The three operations now
use one Goose kernel with separate FP32 rounding for the multiply
and the original order of both adds. The original nodes remain a
fallback. The graph retains the sigmoid producer and the same
expert-ready waits. No two intermediate vectors need to be written.
The server log confirms the new path ran. On sm_89 the entry uses
14 registers and has no spills.

The candidate changes only `cuda-driver.o` with the embedded
device images in the accumulated EXP-1716 archive. The host exchange
buffer, four-head attention, and short down matvec remain present.

## Warm full benchmark

RTX 4070, Apex-35B-V1.0 Q4_K_M, 32,768 input and 512 output tokens,
4,096/4,096 batch and microbatch, context 262,144, Q4_0 KV,
6,290 MiB expert bank, 210 MiB temporary workspace, no probability rows.
Each unprofiled arm ran one full warm-up and one measured request
in the same process. Prefix reuse was zero.

| Metric | EXP-1716 fresh control | EXP-1717 |
|---|---:|---:|
| Prefill, tok/s | 2996.08 | 2998.15 |
| Decode, tok/s | 45.80 | 46.39 |
| Decode, ms | 11157.207 | 11015.913 |
| Client, s | 22.183881 | 22.039037 |
| Peak process VRAM, MiB | 8766 | 8766 |
| Peak RSS, KiB | 21490320 | 21585868 |
| Process swap, KiB | 0 | 0 |
| System swap-out since launch, pages | 0 | 275 |
| System swap-out after readiness, pages | 0 | 0 |

The warm request saves 141.293 ms in decode and 144.844 ms at the
client. All 512 token IDs, text and SLRU counters match in warm-up
and measured runs. The candidate's peak RSS is 93.31 MiB higher,
comparable to earlier peaks of both the four-head and two-head
checkpoint binaries. This pair does not establish the cause.

Keep EXP-1717 as an experimental cumulative checkpoint, with EXP-1716
available for rollback. The acceptance target remains 71.17 decode
tok/s at 32,768 input and 512 output tokens.

The exact source, device images, raw pair and reproducer are under
`build/exp1717-shared-residual-fusion`. Candidate executable SHA-256
is `760198fe6cae7a11cb9f1cd115066ecf745cc1fcc6af8dfed289822c3100073f`.
The patches apply to the cumulative EXP-1713 driver and glue and the
EXP-1665 extracted vector fragment. The base archive is EXP-1716.
