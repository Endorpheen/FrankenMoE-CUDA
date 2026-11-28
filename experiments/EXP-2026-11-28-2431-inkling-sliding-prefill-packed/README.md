# EXP-2026-11-28-2431 — Reuse packed Q4 attention in Inkling sliding prefill

The grouped sliding prefill entry now selects the existing packed
Q4 attention path. It uses paired K/V dequantization, explicit V
accumulators, warp-level query tiles and the existing packed
softmax, with fully masked key tiles omitted. EXP-2426 grouping,
per-chunk windows, merge and scratch sizes are preserved. The SM89
entry changes from 44 registers and a 64-byte stack to 90 registers
and no stack, with the same 3264 bytes of shared memory.

All five fixtures matched partials, metadata and merged outputs
bit for bit. The inputs include positive and negative zero scales
and binary16 subnormals. With a full sliding history and 1024 queries,
the median attention-plus-merge time falls from 4.695 to 4.414 ms.

## Full first-request comparison

RTX 4070, Inkling 2 Mini AD-IQ4_XS, 32768 prompt and 512 output tokens,
context 36864, batch/ubatch 4096/4096, Q4_0 KV and expert cache 6720 MiB.
Each fresh process used a warmed model file cache and computed the
full prompt. The candidate ran before and after the EXP-2430 control.

| Run | Prefill tok/s | Decode tok/s | Client s | Peak RSS KiB | Peak process VRAM MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| Candidate 1 | 567.14 | 48.76 | 68.597 | 13570492 | 9822 |
| EXP-2430 control | 563.53 | 48.80 | 68.966 | 13570644 | 9822 |
| Candidate 2 | 567.71 | 49.52 | 68.378 | 13571228 | 9822 |
| Candidate mean | 567.43 | 49.14 | 68.488 | | |

Mean prefill improves 0.69% and client time decreases by 0.48 s.
The observed decode mean is 0.70% above the immediate control.
The changed path is prefill only. All 512 IDs, text and stop type
match EXP-2128. All runs have 32 draft groups, no rejects and
29363503104 expert H2D bytes. Scored disk reads, process swap and
system swap-out are zero. The 1000 prefill and 60 decode goals remain
open.

Only the native Inkling Flash image was replaced. The CUDA build
used Fried Goose v0.3.0-preview Keelwing, Clang 18 and ptxas 12.6.

| Artifact | SHA-256 |
| --- | --- |
| EXP-2430 executable | `6b36cf501f0fc6ab144c4cb1747d6c6ff2791c0d17dfdc5bb1e47c63b2d576af` |
| Candidate executable | `09b38be45150ebb3b819d37a3f63c8583b2537d9fe119fef9bb734313a6b90b6` |
| Candidate native Flash cubin | `ae0cb1941eb1e4dfec0362a843833da185641be04e7ab4694b6c0365332f7b11` |
| Frozen request | `a0e3e565f497045383aca3d431252780990c28498041a01de7ce2176c028f99f` |
| Candidate 1 response | `c91e6ac2ef522de743913cd5dc4a142250039444c687614e82f81a6b668dc713` |
| EXP-2430 response | `08dc69aa2d6c9c6cca0180d10785ab5c31fc76ba8982b5e899d74eb71136cec3` |
| Candidate 2 response | `33861e2b6e6eaa38318573ddfce3c9f730717f9d5c3386169d89136fecbee24f` |
