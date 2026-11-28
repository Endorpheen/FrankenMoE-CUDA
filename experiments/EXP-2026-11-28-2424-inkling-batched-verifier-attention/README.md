# EXP-2026-11-28-2424 — Batch exact Inkling verifier attention launches

For a group of exact verifier queries, launch independent single-query
Q4 attention blocks together and merge their outputs in one launch.
Each block still uses its query's original K/V range, partitions,
relative scores, softmax and accumulation order. The host groups
queries only when their partition counts match and the existing
partial buffer has sufficient capacity. All other shapes keep their
previous dispatch. The new SM89 entry uses 62 registers per thread,
3264 bytes of shared memory and no local stack.

The component probe compares serial and grouped partials, metadata
and merged FP32 outputs bit for bit. All six cases match. Each timing
is the median of four CUDA-event observations of eight warm
attention-plus-merge sequences.

| First key count | Queries | Sliding window | Serial ms | Grouped ms |
| ---: | ---: | ---: | ---: | ---: |
| 1009 | 16 | 1024 | 1.617 | 0.355 |
| 1024 | 16 | 1024 | 1.627 | 0.318 |
| 1035 | 3 | 0 | 0.301 | 0.120 |
| 8193 | 16 | 0 | 3.290 | 1.648 |
| 32769 | 16 | 0 | 8.914 | 6.399 |
| 33267 | 16 | 0 | 9.049 | 6.463 |

The earlier migrated CUDA trace shows 24576 single-query attention
launches and 24576 merge launches during exact verification. Batching
can remove up to 46080 launches on the same workload. In that earlier
trace 1536 synchronous 36864-byte H2D calls took 6.48 s on the host
and 3.90 ms on the copy engine. Host wait and GPU activity overlap.

## Full first-request comparison

RTX 4070, Inkling 2 Mini AD-IQ4_XS, 32768 prompt and 512 output tokens,
context 36864, batch/ubatch 4096/4096, Q4_0 KV and expert cache 6720 MiB.
Each run starts in a fresh process with a warmed model file cache and
evaluates the whole prompt. The candidate was measured before and after
the EXP-2423 control.

| Run | Prefill tok/s | Decode tok/s | Client s | Peak RSS KiB | Peak process VRAM MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| Candidate 1 | 464.40 | 43.24 | 82.725 | 13570480 | 9822 |
| EXP-2423 control | 468.13 | 35.99 | 84.527 | 13570968 | 9822 |
| Candidate 2 | 463.55 | 42.86 | 82.947 | 13572392 | 9822 |
| Candidate mean | 463.98 | 43.05 | 82.836 | | |

Mean decode improved 19.61%, prefill declined 0.89%, and client time
decreased 2.00% against the immediate control. All runs match the frozen
EXP-2128 output in all 512 IDs, text and stop type. All ran 32 draft
groups, no rejects and 29363503104 expert H2D bytes, with zero scored
disk reads, process swap and system swap-out. The candidate meets the
current acceptance rule. The 800 prefill and 50 decode tok/s goals
remain open.

The driver build used Fried Goose `d682b183` and GCC 13. The CUDA
image build used Fried Goose v0.3.0-preview Keelwing, Clang 18 and
ptxas 12.6. Only the native Inkling Flash CUDA image header changed.

| Artifact | SHA-256 |
| --- | --- |
| EXP-2423 executable | `e86e259babe3f7e763f12242bf8626f551263926b16dd2fee01b7efe1fc28895` |
| Candidate executable | `89dc976fe610690c69dfb98ea53929df18d8029c10b777afae1fec275a846371` |
| Candidate native Flash cubin | `53b9624b563fe92a6f5f43391b06d5be9806efa119af5501337c252f7c2c761f` |
| Frozen request | `a0e3e565f497045383aca3d431252780990c28498041a01de7ce2176c028f99f` |
| Candidate 1 response | `e6cf355efbef5a3e66c1a1396df6bea82022256df4acb9ec53a879e82189c33d` |
| EXP-2423 response | `791b15c17afbb838f566e58e09501282850022f8e59d88b64a66424ef250709a` |
| Candidate 2 response | `6f84f531e51100c898f9af377d341c372430e96c7a2069934969658430a2af92` |
