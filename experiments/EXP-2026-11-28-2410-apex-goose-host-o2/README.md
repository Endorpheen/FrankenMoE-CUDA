# EXP-2026-11-28-2410 — Optimize the emitted Goose CUDA driver on Linux

The Linux recipe compiles `driver-tls.c`, the emitted Goose host logic,
with GCC 13 `-O2`. The CUDA device images, driver glue, diagnostic object,
assembly ABI object, model placement, and server inputs match EXP-2408.
The Windows build keeps its original optimization level.

## Apex verification

RTX 4070, Apex Q4_K_M, 32,768 input and 512 output tokens, context 262144,
batch/ubatch 4096/4096, Q4_0 KV, GPU expert cache 6290 MiB, threads 8/16.
Each scored request followed a full warm-up, with zero cached prompt
tokens and no profiler. The first control was also used for an intervening
rejected GDN-only screen. The remaining scored order was B1, B2, A2.
Every scored request matched all 512 token IDs, text and stop type.

| Arm | Prefill, tok/s | Decode, tok/s | Client, s | Peak RSS, KiB | Sampled process VRAM, MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A1 `-O1` | 3364.60 | 72.62 | 16.869 | 21509772 | 8862 |
| B1 `-O2` | 3341.12 | 75.95 | 16.636 | 21443620 | 8864 |
| B2 `-O2` | 3357.01 | 77.73 | 16.429 | 21507732 | 8864 |
| A2 `-O1` | 3337.33 | 73.09 | 16.905 | 21506500 | 8860 |

Decode improved in both pairs. Means changed from 3350.96 to 3349.07
prefill tok/s (−0.06%) and 72.85 to 76.84 decode tok/s (+5.47%). Mean
request time dropped by 0.355 seconds. Both pairs stayed within a 1% prefill
deviation. Process swap and scored-window system swap-out were zero in
all arms. Sampled process VRAM increased by 2–4 MiB. Maximum pair-set RSS
was 2040 KiB lower for B. This is an incremental Apex improvement.
The absolute EXP-1880 gates and the newer +25% targets remain open.

| Artifact | SHA-256 |
| --- | --- |
| Control executable | `51d5aad111b45d07f4d54b02cef586c8ecf3880900ca5972c2dded25bb7c1632` |
| Candidate executable | `73f9b2e357b2a4baaf0df654c037a441afb26643d1f08b81eb27abf0eea2ae58` |
| Shared generated source | `94e1c8bd05d908a2b43e1cd87aef35e731d58d572fc554250aa17f0e404a5da7` |
| Frozen request | `d81f501a24aa29e1933662fbb8b05d3a1eec5bf7709c622fe7ec63371d8d2d9f` |
| A1 response | `f3d8480acd8bbb9ab3915c5b684f9d26c8289aebd75ef46c39384ccf402173b1` |
| B1 response | `fb7d6da5a885b725f683d0112ce2e2a241f9157d43114a8987117f8db189098c` |
| B2 response | `c254a6a03b55cf1a7c33187731f8b66d71178b35ca6ad9da88a7c20a3cad9c47` |
| A2 response | `703f151e5db6e12428e1b410f4eaa8d62306b401590af3911316bad6cabe649e` |
