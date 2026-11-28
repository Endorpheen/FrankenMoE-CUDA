# EXP-2026-11-28-2439 — Pipeline shared Q8 activation tiles for Inkling

Keep the EXP-2437 split-half formats and all MMA accumulations.
Cooperatively stage both activation components once per CUDA block
for every 64 channels. Pad each shared input row to 72 elements
and copy the next tile with `cp.async` during the current tile's
MMA. Keep the original global workspace and write the same job
offsets. The SM89 kernel uses 18 KiB shared memory and 80 registers
per thread, compared with 72 registers and no shared memory for
the old entry. Neither version has a local stack. Disable the
shared tile pipeline with `I2_Q8_SHARED_TILES=0`.

All four component output buffers match EXP-2437 bit for bit.
Each timed component includes the same input packing and weight
expansion. Results are medians of four batches of twelve launches,
with three warm-up iterations per batch.

| Input width | Output rows | Control ms | Shared tiles ms |
| ---: | ---: | ---: | ---: |
| 2304 | 3072 | 6.305 | 5.005 |
| 2304 | 1024 | 2.185 | 1.681 |
| 3072 | 2304 | 6.413 | 4.726 |
| 2304 | 384 | 0.937 | 0.776 |

## Full first-request comparison

RTX 4070, Inkling 2 Mini AD-IQ4_XS, 32768 prompt and 512 output tokens,
context 36864, batch/ubatch 4096/4096, Q4_0 KV and expert cache 6720 MiB.
Each fresh process used a warmed model file cache and evaluated the
full prompt. Both arms use the same binary. The control disables
only the shared tile entry.

| Run | Prefill tok/s | Decode tok/s | Client s | Peak RSS KiB | Peak process VRAM MiB |
| --- | ---: | ---: | ---: | ---: |
| Candidate 1 | 649.00 | 52.83 | 60.486 | 13542012 | 9822 |
| Control | 618.59 | 53.04 | 62.934 | 13541548 | 9822 |
| Candidate 2 | 650.17 | 51.83 | 60.602 | 13540772 | 9822 |
| Candidate mean | 649.58 | 52.33 | 60.544 | | |

Mean prefill improves 5.01%, with a 1.34% observed decode decline.
Client time decreases by 2.39 s. All 512 IDs, text and stop type
match the frozen EXP-2128 workload. All runs use 32 draft groups
with no rejects and transfer the same 27819417600 expert bytes.
Scored disk reads, process swap and system swap-out are zero.
The prefill gain exceeds the observed decode decline under the
current acceptance rule. The 1000/60 tok/s goal remains open.

The driver build used Fried Goose `d682b183` and GCC 13. The CUDA
image build used Fried Goose v0.3.0-preview Keelwing, Clang 18 and
ptxas 12.6.

| Artifact | SHA-256 |
| --- | --- |
| Candidate/control executable | `678686b53426f6ab21b47655b749c264fdb94ba3455fcb60faee25d04a6bd261` |
| Candidate grouped MoE CUDA image | `aa3dfd8a9e335f2a0bbbe66044de9404a773dac91a7bd334a8701fa2948ff1f0` |
| Frozen request | `a0e3e565f497045383aca3d431252780990c28498041a01de7ce2176c028f99f` |
| Candidate 1 response | `2348beca4a9d8ce5dbe19d34c497fd55ff818520c3eaf972f50fbcc06564edaf` |
| Control response | `017f1227762f231a12fe5f2fe6761b0931b79007ff60a6ccecb34c3d9b1a9681` |
| Candidate 2 response | `f18ccfd5fc8b00ec10c45a1b1fb8aeaab9a6bd472bf9de4ffc8a5f7af42bf0` |
