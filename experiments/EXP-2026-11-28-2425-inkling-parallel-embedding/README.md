# EXP-2026-11-28-2425 — Parallelize Inkling prefill embedding rows

Reuse the existing Inkling prefill CPU team for embedding preparation.
Each worker handles a disjoint portion of the existing output buffer
and calls the original Q8_0 unpack and RMS normalization for each row.
The original serial path remains active for small batches and when
the team is disabled. The CUDA inputs, launches and buffers are unchanged.

The EXP-2424 CUDA trace shows seven inter-batch GPU gaps totalling
10.07 s between a MoE epilogue and the next 36 MiB input upload. Its
prefill records 54.76 s in kernel intervals and 12.45 s outside kernel
and copy intervals. The profiler reported possible loss when tracing
stopped after the response. Its decode totals are partial. A separate
CPU profile captured 11164 samples without loss. The embedding export
accounts for about 4.74 sampled CPU seconds in that profile. The
sampled export time alone does not explain the full wall-clock gain.

## Full first-request comparison

RTX 4070, Inkling 2 Mini AD-IQ4_XS, 32768 prompt and 512 output tokens,
context 36864, batch/ubatch 4096/4096, Q4_0 KV and expert cache 6720 MiB.
Each process starts with a warmed model file cache and computes the
entire prompt. The candidate ran before and after the EXP-2424 control.

| Run | Prefill tok/s | Decode tok/s | Client s | Peak RSS KiB | Peak process VRAM MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| Candidate 1 | 535.57 | 43.39 | 73.290 | 13569908 | 9822 |
| EXP-2424 control | 462.03 | 42.81 | 83.191 | 13571600 | 9822 |
| Candidate 2 | 535.42 | 42.91 | 73.447 | 13570860 | 9822 |
| Candidate mean | 535.49 | 43.15 | 73.368 | | |

Mean prefill improved 15.90%, observed decode improved 0.79%,
and mean client time declined by 9.82 s. All runs match the frozen
EXP-2128 response in all 512 token IDs, text and stop type. All had
32 draft groups, 29363503104 expert H2D bytes and zero draft rejects.
Scored disk reads, process swap and system swap-out were zero.
The 800 prefill and 50 decode tok/s goals remain open.

The Inkling host backend was built with Fried Goose `d682b183` and
GCC 13. All other link inputs were reused from EXP-2424.

| Artifact | SHA-256 |
| --- | --- |
| EXP-2424 executable | `89dc976fe610690c69dfb98ea53929df18d8029c10b777afae1fec275a846371` |
| Candidate executable | `0abe4f773f70671c510485e97b0a01bc894fd6578369e81ca6042908c03473fa` |
| Candidate host backend object | `9c6d9b923f93abd29dca072755e83472da53b39d18e047cb7143a063935f67b1` |
| Frozen request | `a0e3e565f497045383aca3d431252780990c28498041a01de7ce2176c028f99f` |
| Candidate 1 response | `61a7348aaee4154662d0270796441e06132eab8a7dcdd525920996820bd969e2` |
| EXP-2424 response | `5d8859c18fee3968c06cb95fcb5db68e7b2a59fd16d54971667a5c749aa23b59` |
| Candidate 2 response | `0ff90b9ece88fdd2a8edc216959e5c3a3b88df7b83e2c925cf485280eb429002` |
