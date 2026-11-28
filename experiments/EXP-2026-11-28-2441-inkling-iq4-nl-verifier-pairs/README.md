# EXP-2026-11-28-2441 — Reuse IQ4_NL weights across Inkling verifier jobs

Transfer [EXP-2427 operand reuse](../EXP-2026-11-28-2427-inkling-iq4-verifier-pairs/README.md)
to the IQ4_NL down projection. The exact 1152-input verifier
uses a specialized two-job entry for batches of at least 64 jobs.
It decodes each weight once per adjacent pair that shares an expert
plane. Mismatched pairs and odd tails use the original single-job
operation. Each output preserves the original FP32 recurrence,
reduction and scatter offset. The candidate uses 38 registers
against 40, with no stack in either. Set
`I2_VERIFY_IQ4_NL_PAIR=0` to use the original entry.

All six component output buffers match bit for bit. The dedicated
entry takes 0.491 ms instead of 0.807 ms at 128 jobs with 64
matching pairs. At 32 matching pairs the medians are 0.697 ms
and 0.807 ms. The all-distinct case costs 0.833 ms against 0.785 ms.
Times are medians of four batches of 20 warmed launches.

## Full first-request comparison

RTX 4070, Inkling 2 Mini AD-IQ4_XS, 32768 prompt and 512 output tokens,
context 36864, batch/ubatch 4096/4096, Q4_0 KV and expert cache 6720 MiB.
Each fresh process used a warmed model file cache and evaluated the
entire prompt. The same executable ran both arms. The control
disabled only the paired IQ4_NL path.

| Run | Prefill tok/s | Decode tok/s | Client s | Peak RSS KiB | Peak process VRAM MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| Candidate 1 | 651.35 | 54.00 | 60.133 | 13540896 | 9822 |
| Control | 652.42 | 52.44 | 60.309 | 13541896 | 9822 |
| Candidate 2 | 648.30 | 54.35 | 60.270 | 13542576 | 9822 |
| Candidate mean | 649.83 | 54.17 | 60.201 | | |

Decode improves 3.31% and observed prefill declines 0.40%.
Mean client time falls 0.11 s. All 512 IDs, text and stop
type match EXP-2128. Each run has 32 draft groups, zero rejects
and 27819417600 expert H2D bytes. No scored disk reads,
process swap or system swap-out were observed. The 1000/60
tok/s targets remain open.

The driver used Fried Goose `d682b183` and GCC 13. The CUDA
image used Fried Goose v0.3.0-preview Keelwing, Clang 18 and
ptxas 12.6.

| Artifact | SHA-256 |
| --- | --- |
| Candidate/control executable | `b3c1124f2f6c85763f3a6ebd88530157c808e06c7345bb9508f9f3c200b72fae` |
| Scoped matrix cubin | `e647af905252bb6f35cdf97290212c687f28955900f95ce8407d408bb63190ce` |
| Frozen request | `a0e3e565f497045383aca3d431252780990c28498041a01de7ce2176c028f99f` |
| Candidate 1 response | `d5db25a638c40f07a83c4c7de714a238dee5bf8177bfc8125f1e039bf7bea861` |
| Control response | `511762f520f5e322960e457a621abcf2e872dcf408e854f3b55be93e3ba22507` |
| Candidate 2 response | `2a824b36f3a07bfb949ad66a51b0c119630696f7ad2ed2fd5e37c1a9ee8d235f` |
