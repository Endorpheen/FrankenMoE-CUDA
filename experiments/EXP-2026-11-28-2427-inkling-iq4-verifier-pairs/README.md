# EXP-2026-11-28-2427 — Reuse decoded IQ4_XS weights in the Inkling verifier

Apply the [EXP-2423 operand reuse principle](../EXP-2026-11-28-2423-inkling-q8-row-pair/README.md)
to expert weights. The verifier already groups jobs by expert plane.
Process two adjacent jobs per warp for the measured 2304-input
IQ4_XS verifier geometry with at least 64 jobs. When both jobs
reference the same plane, decode a weight value once and use it in
two independent dot products. Different planes and an odd tail use
the existing dot product. Each output keeps its FP32 recurrence
and warp reduction. No new workspace is allocated. The SM89 pair
entry uses 39 registers versus 40, with no stack in either entry.

All six component cases matched the original output bit for bit.
Times are medians of four observations of 20 warmed CUDA launches,
on 2304-by-2304 expert planes with independently ordered inputs.

| Jobs | Same-plane pairs | Control ms | Candidate ms |
| ---: | ---: | ---: | ---: |
| 128 | 0 | 1.868 | 1.896 |
| 128 | 16 | 1.831 | 1.766 |
| 128 | 32 | 1.787 | 1.664 |
| 128 | 48 | 1.779 | 1.558 |
| 128 | 64 | 1.733 | 1.425 |
| 127 | 31 | 1.774 | 1.648 |

## Full first-request comparison

RTX 4070, Inkling 2 Mini AD-IQ4_XS, 32768 prompt and 512 output tokens,
context 36864, batch/ubatch 4096/4096, Q4_0 KV and expert cache 6720 MiB.
Each fresh process used a warmed model file cache and computed the
full prompt. The candidate ran before and after the EXP-2426 control.

| Run | Prefill tok/s | Decode tok/s | Client s | Peak RSS KiB | Peak process VRAM MiB |
| --- | ---: | ---: | ---: | ---: |
| Candidate 1 | 549.67 | 45.89 | 71.069 | 13571460 | 9822 |
| EXP-2426 control | 548.18 | 43.55 | 71.854 | 13572184 | 9822 |
| Candidate 2 | 548.86 | 45.59 | 71.233 | 13571728 | 9822 |
| Candidate mean | 549.26 | 45.74 | 71.151 | | |

Mean decode improves 5.03% and measured prefill improves 0.20%.
All 512 IDs, text and stop type match frozen EXP-2128. All runs have
32 draft groups, no draft rejects and 29363503104 expert H2D bytes.
Scored disk reads, process swap and system swap-out are zero.

Both candidate runs exceed the frozen EXP-2128 rates of 436.58 prefill
and 35.99 decode tok/s by at least 25% simultaneously. The candidate
mean gains are 25.81% and 27.07%. The 800 prefill and 50 decode tok/s
goals remain open.

The driver build used Fried Goose `d682b183` and GCC 13. The matrix
image was built from Goose v0.3.0-preview Keelwing with Clang 18 and
ptxas 12.6. The only replaced embedded image is the Inkling matrix
image.

| Artifact | SHA-256 |
| --- | --- |
| EXP-2426 executable | `3650cd4950dc64847b0c814b99c47f26c29a6b3881c084105a7ac750a6334a46` |
| Candidate executable | `a01abc0ce372e61cfd6dc7f1da5d4d53fe0962673a4dc077bda398f0086d5999` |
| Candidate matrix cubin | `72d4d14c2ec0e2d816339175a552a0182456d4bcdb89f6ce11c4a05c3cc30774` |
| Frozen request | `a0e3e565f497045383aca3d431252780990c28498041a01de7ce2176c028f99f` |
| Candidate 1 response | `c400982f079413426c60c90cecc592d9f6ea056d654f33efebe74443a84e05de` |
| EXP-2426 response | `1c3a1a788954e8e84bd6229397af317889384f8f196ecf7538dc090d067ddf56` |
| Candidate 2 response | `41036c87c929cca8359bc58414f498b17a564f3fe818d0ef3a5e7cc9cd471fdd` |
