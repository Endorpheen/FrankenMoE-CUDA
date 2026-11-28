# EXP-2026-11-28-2422 — Unroll Inkling Q4 warp decode value accumulation

Reuse the explicit four-way Goose V accumulator from EXP-2421 in the
single-query path that already loads paired Q4 values. The CUDA launch,
arithmetic order, softmax and workspace are unchanged. The SM89 decode
entry changes from 48 registers and a 64-byte stack to 64 registers and
zero stack. Among the rebuilt image headers only the native Inkling Flash
image differs. The packed prefill image is byte-identical.

The component probe compared the partial and metadata buffers bit for
bit across five key lengths, including sliding attention and partial key
tiles. All matched. Each timing observation used 64 launches after three
warm-up launches for that arm. The table gives the median of four
observations per arm. It measures continuous kernel launches without
the rest of the inference graph.

| Keys | Window | EXP-2421 µs | EXP-2422 µs |
| ---: | ---: | ---: | ---: |
| 1024 | 1024 | 104.61 | 95.94 |
| 1025 | 1024 | 106.54 | 97.60 |
| 2048 | 0 | 189.95 | 162.92 |
| 8201 | 0 | 228.82 | 188.17 |
| 32768 | 0 | 740.49 | 564.70 |

## Full first-request comparison

RTX 4070, Inkling 2 Mini AD-IQ4_XS, 32768 prompt and 512 output tokens,
context 36864, batch/ubatch 4096/4096, Q4_0 KV and expert cache 6720 MiB.
Each run used a fresh process and a warmed model file cache and evaluated
the entire prompt. The score excludes the intermediate diagnostic run
in which 40 system swap pages were written.

| Clean run | Prefill tok/s | Decode tok/s | Client s | Peak RSS KiB | Peak process VRAM MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| Candidate 1 | 441.82 | 33.40 | 89.806 | 13571116 | 9822 |
| EXP-2421 control 1 | 441.84 | 33.67 | 89.678 | 13571024 | 9822 |
| Candidate 2 | 440.86 | 35.71 | 88.980 | 13571732 | 9822 |
| EXP-2421 control 2 | 441.69 | 34.00 | 89.563 | 13571368 | 9822 |
| EXP-2421 mean | 441.76 | 33.84 | 89.620 | | |
| EXP-2422 mean | 441.34 | 34.55 | 89.393 | | |

Mean decode improved by 2.12% with a 0.10% prefill decline. The two
paired decode differences were −0.81% and +5.01%. The sign varied
between pairs. The clean aggregate, the exact component comparison
and the isolated kernel speedup support adopting this candidate under
the current acceptance rule. The comparable full request shows a
smaller gain than the isolated kernel. All runs kept the same 32 draft
groups and 29363503104 expert H2D bytes, with no draft rejection.
Every run matched the frozen EXP-2128 token IDs, text and stop type.
The clean runs recorded zero scored disk reads, process swap and
system swap-out. The simultaneous 25% targets remain open.

| Artifact | SHA-256 |
| --- | --- |
| EXP-2421 control executable | `e65609d28f60c7e59fe0d30300c09e051c6c5785d869a8c19acc22a98b2e0fbe` |
| Candidate executable | `e3dbf06ff169ad3c237a7f924c3b3adc6449040bcb996201e5f3c31918bacec8` |
| Candidate native Flash cubin | `1eb0370804489b982e3b41958a161c0844746af8fd1b1da479e210be18689f6a` |
| Frozen request | `a0e3e565f497045383aca3d431252780990c28498041a01de7ce2176c028f99f` |
| Candidate 1 response | `1b5f4fa0a5234b73b58502e57559cae80381504d4a072803bf9944342de8631d` |
| EXP-2421 control 1 response | `b3b7eaba761cd0fced109830eae99727e0f28b42e3517cc713c3f8a68c249294` |
| Candidate 2 response | `ca2e90561b4e18f42565a0503949d6c5ec886795336c8313e4a58729d679cfe9` |
| EXP-2421 control 2 response | `24b7a1b680151c869651c45ae9445e311d6ebeaddaa844cb66e54525297b99d7` |
