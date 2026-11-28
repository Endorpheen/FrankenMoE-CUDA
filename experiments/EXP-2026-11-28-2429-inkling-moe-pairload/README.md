# EXP-2026-11-28-2429 — Share packed expert loads across MMA steps

Grouped Inkling IQ4_XS and IQ4_NL projections now read two aligned
16-bit code pairs and their scale once per 32 channels. Each of the
two original 16-channel MMA steps extracts its own nibbles from the
loaded words. The FP32 coefficient products, FP16 conversion, MMA
order and output offsets are preserved. The kernel uses the existing
launch and scratch. The large SM89 entries use 62 registers per
thread, up from 56 (IQ4_XS) and 53 (IQ4_NL), with no local stack.

The component probe tested each of the 8-, 16- and 32-job entries
on partial tiles and a partial output-row block. It also tested the
32-job entries on 2304 rows, 32795 jobs and 1085 descriptors.
All eight output buffers match the reference bit for bit. Table
values are medians of four observations of eight warm launches.

| Large shape | EXP-2427 ms | Candidate ms | Time reduction |
| --- | ---: | ---: | ---: |
| IQ4_XS, width 2304 | 26.985 | 23.903 | 11.42% |
| IQ4_NL, width 1152 | 12.186 | 11.300 | 7.28% |

The separate EXP-2428 screen reused compensated TF32 dense MMA for
expanded Inkling Q8 projections. Its component error passed the
defined bounds, and its four shapes ran 2.4–3.1 times slower than
the accepted Q8 path. That experiment ended at the device gate.

## Full first-request comparison

RTX 4070, Inkling 2 Mini AD-IQ4_XS, 32768 prompt and 512 output tokens,
context 36864, batch/ubatch 4096/4096, Q4_0 KV and expert cache 6720 MiB.
Each fresh process used a warmed model file cache and evaluated the
full prompt. The candidate ran before and after the EXP-2427 control.

| Run | Prefill tok/s | Decode tok/s | Client s | Peak RSS KiB | Peak process VRAM MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| Candidate 1 | 565.12 | 46.09 | 69.413 | 13570712 | 9822 |
| EXP-2427 control | 549.35 | 45.70 | 71.152 | 13570876 | 9822 |
| Candidate 2 | 563.80 | 45.91 | 69.582 | 13571752 | 9822 |
| Candidate mean | 564.46 | 46.00 | 69.498 | | |

Mean prefill improves 2.75%, observed decode improves 0.65% and client
time decreases by 1.65 s. All 512 IDs, text and stop type match frozen
EXP-2128. All runs have 32 draft groups, no draft rejects and
29363503104 expert H2D bytes. Scored disk reads, process swap and
system swap-out are zero. The 800 prefill and 50 decode goals remain
open.

Only the Inkling grouped MoE image was replaced. The device build used
Fried Goose v0.3.0-preview Keelwing, Clang 18 and ptxas 12.6.

| Artifact | SHA-256 |
| --- | --- |
| EXP-2427 executable | `a01abc0ce372e61cfd6dc7f1da5d4d53fe0962673a4dc077bda398f0086d5999` |
| Candidate executable | `c70b696221c7ac240c5e805610189e083e77d1526fc5c723a8db5460095b9501` |
| Candidate grouped MoE cubin | `151c11e428460183eced5fce4e276ba7b026f045382ed2e493d587b4b150f0c6` |
| Frozen request | `a0e3e565f497045383aca3d431252780990c28498041a01de7ce2176c028f99f` |
| Candidate 1 response | `697a43d63685429687546dfcb4dc6a38ed1eea28c3c0735434ff707234f606a9` |
| EXP-2427 response | `dd2a82a01ec074c9ae80107fe8e7cdbfcb333c63a880cbd1f18b8d91da9200c7` |
| Candidate 2 response | `87e918ba426532b8e1a60e41ecd67490c6e7e83e61a6e58103ca239b96f6fa7e` |
