# EXP-2026-11-28-2442 — Pipeline shared activation tiles for Inkling IQ4 MoE

Transfer [EXP-2439's shared activation staging](../EXP-2026-11-28-2439-inkling-q8-shared-tiles/README.md)
to the grouped IQ4_XS and IQ4_NL prefill kernels. For the full
32-job, 64-row tiles, each block stages the 64-channel activation
tile once, while the other shared buffer is used by the four
MMA warps. Preserve the EXP-2429 packed weight loads and the
existing MMA order and scatter. Partial tiles use the original
path. The SM89 entries use 64 registers and 9216 bytes of shared
memory with no stack. Global workspace is unchanged. Disable
the new kernels with `I2_MOE_SHARED_TILES=0`.

The component outputs match byte for byte in both quant formats
at 259 rows with partial job and row tiles and at 2304 rows with
32795 jobs and 1085 mixed descriptors. Each timing is the median
of four batches of eight warmed CUDA launches.

| Shape | Control ms | Candidate ms | Time reduction |
| --- | ---: | ---: | ---: |
| Large IQ4_XS | 23.783 | 15.875 | 33.25% |
| Large IQ4_NL | 12.061 | 6.538 | 45.79% |
| Partial IQ4_XS | 0.130 | 0.126 | 2.76% |
| Partial IQ4_NL | 0.059 | 0.056 | 5.04% |

## Full first-request comparison

RTX 4070, Inkling 2 Mini AD-IQ4_XS, 32768 prompt and 512 output tokens,
context 36864, batch/ubatch 4096/4096, Q4_0 KV and expert cache 6720 MiB.
Each fresh process used a warmed model file cache and evaluated
the full prompt. Both arms use the same binary. The control
disables only the new IQ4 shared tile kernels.

| Run | Prefill tok/s | Decode tok/s | Client s | Peak RSS KiB | Peak process VRAM MiB |
| --- | ---: | ---: | ---: | ---: |
| Candidate 1 | 721.27 | 55.13 | 55.036 | 13542108 | 9822 |
| Control | 652.16 | 53.12 | 60.189 | 13542212 | 9822 |
| Candidate 2 | 720.39 | 53.62 | 55.345 | 13541244 | 9822 |
| Candidate mean | 720.83 | 54.38 | 55.190 | | |

Mean prefill improves 10.53% and mean observed decode improves 2.36%.
The optimization changes the prefill path. Client time decreases
by 5.00 s. All 512 IDs, text and stop type match EXP-2128. All
runs use 32 draft groups without rejects and transfer 27819417600
expert bytes in decode. No scored disk reads, process swap or
system swap-out occur. The active 1000/60 tok/s goals remain open.

The driver used Fried Goose `d682b183` and GCC 13. The CUDA
image used Fried Goose v0.3.0-preview Keelwing, Clang 18 and
ptxas 12.6.

| Artifact | SHA-256 |
| --- | --- |
| Candidate/control executable | `8c7681635ea807d8d77731c62d5b154a463b74f8f96d25bb692b99274072b10e` |
| Candidate grouped MoE CUDA image | `0a9139e0dbd748888771245097b1e8c4ae28017a72d02acc953b9bce25162156` |
| Frozen request | `a0e3e565f497045383aca3d431252780990c28498041a01de7ce2176c028f99f` |
| Candidate 1 response | `fdd080859a0edc5a1f3940b32f7a69c59a2e2801dd4edee9dc5600f71a6253ae` |
| Control response | `9d6980acf8cf04753a94436edab9251b245103f1256cc450b9634b763406f428` |
| Candidate 2 response | `69c0d069649cb612058395e97040d29e281d01d1ce68fc139441a7c564882eef` |
