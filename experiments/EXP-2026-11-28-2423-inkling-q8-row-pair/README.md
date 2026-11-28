# EXP-2026-11-28-2423 — Share packed Inkling Q8 activations across output rows

The two expanded FP32 Q8 prefill projections account for 16.79 GPU
seconds in the matched migrated Inkling trace. In the expanded path,
each warp now computes two adjacent output rows. Each packed four-lane
activation vector feeds both rows. The FP32 FMA and warp reduction order
for each row, scatter offsets, weight expansion and scratch sizes are
preserved. The dispatch applies to the existing selected 4096-input
shapes and halves the output-row grid. The paired SM89 entries use 80
registers per thread versus 54, with zero stack in both variants.

The component probe compared full output buffers bit for bit, with
permuted input and output job offsets and a sentinel for unwritten
values. All four shapes matched. The timings below are medians of
four CUDA-event observations of 16 launches after warm-up, excluding
the common input pack and weight expansion.

| Input width | Output rows | Warps/block | Control ms | Candidate ms |
| ---: | ---: | ---: | ---: | ---: |
| 2304 | 3072 | 4 | 11.492 | 9.173 |
| 2304 | 1024 | 4 | 4.405 | 3.111 |
| 3072 | 2304 | 2 | 11.838 | 10.309 |
| 2304 | 384 | 2 | 1.823 | 1.331 |

## Full first-request comparison

RTX 4070, Inkling 2 Mini AD-IQ4_XS, 32768 prompt and 512 output tokens,
context 36864, batch/ubatch 4096/4096, Q4_0 KV and expert cache 6720 MiB.
Each request starts in a fresh process with a warmed model file cache
and computes the full prompt. The candidate was measured before and
after the EXP-2422 control.

| Run | Prefill tok/s | Decode tok/s | Client s | Peak RSS KiB | Peak process VRAM MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| Candidate 1 | 469.28 | 36.36 | 84.209 | 13570492 | 9822 |
| EXP-2422 control | 444.66 | 36.36 | 88.081 | 13570692 | 9822 |
| Candidate 2 | 468.17 | 35.69 | 84.644 | 13570296 | 9822 |
| Candidate mean | 468.72 | 36.03 | 84.427 | | |

Mean candidate prefill is 5.41% faster and decode is 0.92% slower
than the immediate control. All runs match the frozen EXP-2128
response in all 512 IDs, text and stop type. No scored disk reads,
process swap or system swap-out were observed. The candidate meets
the experiment acceptance rule and carries the earlier Inkling
improvements. The current 800 prefill and 50 decode tok/s targets
remain open.

The driver build used Fried Goose `d682b183` and GCC 13. The device
image build used Fried Goose v0.3.0-preview Keelwing, Clang 18 and
ptxas 12.6. The linked candidate uses the EXP-2422 runtime inputs
and CUDA image set with the Q8 image replaced and Goose CUDA driver
rebuilt.

| Artifact | SHA-256 |
| --- | --- |
| EXP-2422 executable | `e3dbf06ff169ad3c237a7f924c3b3adc6449040bcb996201e5f3c31918bacec8` |
| Candidate executable | `e86e259babe3f7e763f12242bf8626f551263926b16dd2fee01b7efe1fc28895` |
| Control Q8 cubin | `56a733623551b938e5c00a9314c7019d2b05f8e18f0528511bafaebefd331108` |
| Candidate Q8 cubin | `a978a1949a5a806ee670a61cac0050f2913a123eb41aa00ccef6dff5b82dde8f` |
| Driver compiler | `ff5764c00fe53755fe01bebf13cc94c6094140aa41a2b4bc3dc77ec4fd3dc80d` |
| Frozen request | `a0e3e565f497045383aca3d431252780990c28498041a01de7ce2176c028f99f` |
| Candidate 1 response | `dc5ba162a6885417a295cc2bad75d9f9402a81a1d9ff3e533c3618ff88451dd3` |
| EXP-2422 response | `e228980835e15054bb194eefa71410acb23b365cde2f34b36aec1c32612f3b76` |
| Candidate 2 response | `439aad145fdbcb22373b69b4841b48bfa8a5f9ee0cc354da9d59423076939897` |
