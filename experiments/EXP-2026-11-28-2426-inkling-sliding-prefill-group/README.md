# EXP-2026-11-28-2426 — Group Inkling sliding prefill attention launches

Apply the [EXP-2424 launch grouping pattern](../EXP-2026-11-28-2424-inkling-batched-verifier-attention/README.md)
to Q4 sliding prefill. Group up to eight existing 128-query chunks
and use one merge launch per group. Each CUDA block retains the
original chunk's key window, query tile and arithmetic. The host
selects 512 or 1024 queries according to already allocated partial
storage. The previous chunk size is retained where the shape or
available workspace does not qualify. The new SM89 entry uses 44
registers, four more than the control, with the same 64-byte stack
and 3264-byte shared allocation.

The component probe matched partials, metadata and merged output bit
for bit on all five cases. Times are medians of four observations
of eight warm attention-plus-merge sequences.

| Initial history | Queries | Control ms | Grouped ms |
| ---: | ---: | ---: | ---: |
| 0 | 512 | 1.025 | 1.000 |
| 640 | 512 | 2.913 | 2.464 |
| 1023 | 512 | 2.946 | 2.427 |
| 1023 | 447 | 2.856 | 2.181 |
| 1023 | 1024 | 6.115 | 4.659 |

## Full first-request comparison

RTX 4070, Inkling 2 Mini AD-IQ4_XS, 32768 prompt and 512 output tokens,
context 36864, batch/ubatch 4096/4096, Q4_0 KV and expert cache 6720 MiB.
Each fresh process used a warmed model file cache and computed the
full prompt. The candidate ran before and after the EXP-2425 control.

| Run | Prefill tok/s | Decode tok/s | Client s | Peak RSS KiB | Peak process VRAM MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| Candidate 1 | 547.44 | 43.38 | 71.963 | 13571156 | 9822 |
| EXP-2425 control | 535.61 | 43.69 | 73.217 | 13571624 | 9822 |
| Candidate 2 | 548.05 | 43.32 | 71.901 | 13570860 | 9822 |
| Candidate mean | 547.74 | 43.35 | 71.932 | | |

Mean candidate prefill improves 2.27% and decode declines 0.78%.
The client time falls by 1.28 s. All 512 IDs, text and stop type
match frozen EXP-2128. Scored disk reads, process swap and system
swap-out are zero. The 800 prefill and 50 decode tok/s goals remain open.

The driver build used Fried Goose `d682b183` and GCC 13. The CUDA image
build used Fried Goose v0.3.0-preview Keelwing, Clang 18 and ptxas 12.6.
Only the native Inkling Flash image header changed.

| Artifact | SHA-256 |
| --- | --- |
| EXP-2425 executable | `0abe4f773f70671c510485e97b0a01bc894fd6578369e81ca6042908c03473fa` |
| Candidate executable | `3650cd4950dc64847b0c814b99c47f26c29a6b3881c084105a7ac750a6334a46` |
| Candidate native Flash cubin | `6d663c8dfcd1281c19513c4174367b102eb0fb987f8f1e8b803e4f4c988169bc` |
| Frozen request | `a0e3e565f497045383aca3d431252780990c28498041a01de7ce2176c028f99f` |
| Candidate 1 response | `391d0402a8ad2832e67562220a2f3cf0b5a39af27a46323c446e9c5d2fd9cdfd` |
| EXP-2425 response | `60f0d078454e9b9c9a3eaec523b58fc15406836a21bf89c6dd16c1ed01a624d5` |
| Candidate 2 response | `45e8bbad494b88f689c5783858e53618c24cf2e607f0795f3f79f7efaa4df5a4` |
