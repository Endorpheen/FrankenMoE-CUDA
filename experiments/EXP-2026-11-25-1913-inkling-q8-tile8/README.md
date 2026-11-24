# EXP-2026-11-25-1913: Reuse one Q8 block across eight Inkling inputs

The existing dense Q8_0 matrix kernel reuses one decoded block across
four independent input rows. The candidate uses eight rows for dense
Inkling batches. Each row keeps the same FP32 additions and warp
reduction order. This avoids FP16 activation packing. The eight-row path
requires at least 16 dense jobs and leaves single-token dispatch unchanged.

The same binary runs both arms on RTX 4070. The input is the frozen
512/16 request from EXP-1911, context 1024, Q4_0 K/V, batch and ubatch 512,
with a 384 MiB arena, retained CPU workers, and the exact sampler.
A uses `I2_Q8_TILE8=0` and B uses `I2_Q8_TILE8=1`.
Use one cold expert-cache request and one repeat for every arm.

| A–B–B–A mean | Four rows | Eight rows |
| --- | ---: | ---: |
| Cold prefill, tok/s | 139.04 | 140.79 |
| Warm prefill, tok/s | 173.66 | 182.57 |
| Cold decode, tok/s | 9.95 | 10.71 |
| Warm decode, tok/s | 10.81 | 10.83 |

Warm prefill improved in both pairs. All 1086 state checks per arm,
generated IDs, text and expert traffic matched the sequential reference.
All four arms peaked at 9822 MiB board VRAM with no swap. RSS stayed
within 1188 KiB across the block. The cold decode spread includes a
slow control arm. The dispatch change is scoped to prefill batches and
this experiment does not credit the cold decode difference.

Apply the source patches with `patch -p1` to the exact previous
quantized-matrix Goose and OpenCL C files and matching driver header.
Use the retained-worker candidate archive as the host baseline, and the
frozen shared-attention driver's input files. The supplied build helper
is intended for the sibling experiment layout. It uses KeelWing, Clang
18, ptxas for SM89 and GCC 13. The GPU image reports no spills.
The isolated exact prefill result does not constitute an Apex regression
gate or a production default change.
