# EXP-2026-11-28-2437 — Use split-half Tensor Core Q8 projections in Inkling prefill

For the existing expanded Q8_0 prefill shapes, pack each decoded
weight and activation as an FP16 leading term and an FP16 residual
scaled by 4096. The Tensor Core kernel uses independent FP32 sums
for the leading products and the two cross-products. The low-low
product is omitted. It combines the two FP32 sums before scattering
to the existing job offsets. Source GGUF blocks, Q4_0 KV and the
four-byte workspace budget per element are preserved. The path is
enabled in the existing 4096-input dispatch and may be disabled
with `I2_Q8_SPLIT_HALF=0`.

The first candidate used arrays for the accumulators and had a
128-byte local stack. Replacing those arrays with four explicit
accumulators removed the stack and raised register use from 40
to 72. Each timed component includes packing, weight expansion
and projection. Results are medians of four observations with
twelve warm launches per sample. All four comparisons meet the
predeclared relative L2 error bound of 2e-5. Intermediate sums
differ from the reference.

| Input width | Output rows | FP32 ms | Split-half ms | Relative L2 error |
| ---: | ---: | ---: | ---: | ---: |
| 2304 | 3072 | 9.590 | 6.480 | 3.77e-6 |
| 2304 | 1024 | 3.104 | 2.168 | 3.77e-6 |
| 3072 | 2304 | 10.440 | 6.157 | 4.98e-6 |
| 2304 | 384 | 1.505 | 0.970 | 3.77e-6 |

## Full first-request comparison

RTX 4070, Inkling 2 Mini AD-IQ4_XS, 32768 prompt and 512 output tokens,
context 36864, batch/ubatch 4096/4096, Q4_0 KV and expert cache 6720 MiB.
Each fresh process used a warmed model file cache and evaluated the
full prompt. The control used the same binary with split-half disabled.

| Run | Prefill tok/s | Decode tok/s | Client s | Peak RSS KiB | Peak process VRAM MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| Split-half 1 | 622.75 | 49.998 | 63.157 | 13540788 | 9822 |
| FP32 control | 567.00 | 48.831 | 68.593 | 13571992 | 9822 |
| Split-half 2 | 619.08 | 49.690 | 63.540 | 13542260 | 9822 |
| Split-half mean | 620.91 | 49.844 | 63.349 | | |

Mean prefill improves 9.51%, observed decode improves 2.08%, and
client time decreases by 5.24 s. All 512 IDs, text and stop type
match EXP-2128 on the frozen request. The draft verifier still
has 32 groups and zero rejects. Decode expert H2D was
27819417600 bytes on both candidate runs versus 29363503104 on
the control. The change in intermediate arithmetic therefore
does not establish parity of expert traffic or internal state.
Scored disk reads, process swap and system swap-out were zero.
The 1000 prefill and 60 decode goals remain open.

The driver build used Fried Goose `d682b183` and GCC 13. The image
used Fried Goose v0.3.0-preview Keelwing, Clang 18 and ptxas 12.6.

| Artifact | SHA-256 |
| --- | --- |
| Candidate executable | `327ecca66da228d09a7b841b98bacd2aec75b3aa319e7b8ec959e1c038f4270e` |
| Candidate grouped MoE image cubin | `de7c9c0a537c2906e2377fbb7deb42adaaed17279dc720da3986cf9b65bf6f30` |
| Frozen request | `a0e3e565f497045383aca3d431252780990c28498041a01de7ce2176c028f99f` |
| Split-half 1 response | `fc82c5066e7694a42d5f38d39a0d5c862f6ecf5a5fa5cd9315fb55bfa01969a4` |
| FP32 control response | `8b8f750648a844fd619254c722ec38168cb25a9fffacd81d5e0f068cb93a0c89` |
| Split-half 2 response | `91953a32c9c79c6f8fd80f6939ec3255b6dcfb3d49dfcda0aa46ce4bc5e48198` |
