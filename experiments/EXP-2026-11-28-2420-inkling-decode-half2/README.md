# EXP-2026-11-28-2420 — Restore Inkling allocation and pair Q4 decode loads

The migrated runtime tripped its alignment guard when `reserve` passed the
address field of a packed Inkling KV slot to an allocator expecting an
aligned `u64` output. The driver allocates through a local aligned value and
publishes the address and capacity together after success. The alignment
guard remains enabled. The corrected runtime completed 4096/2 under GDB
and the frozen 32K/512 workload in an ordinary process.

CUDA traces comparing the unchanged EXP-2128 image and the aligned Goose
build recorded identical copies and launch counts. The 24576 launches of
`goose_i2_flash_mma_decode` grew from 3915.26 to 6056.37 ms. The active
prefill `i2_flash_prefill_all_softmax` grew from 6276.53 to 7831.51 ms at
2048 calls. Kernel times and blocking host API times overlap.

The Q4 decode kernel now reuses the existing prefill half2 helpers for
K and V dequantization. It keeps the decode softmax, accumulation order,
parameters and buffer allocation. Only the native Inkling Flash image
changed among the rebuilt CUDA image headers. Five component comparisons
matched every FP32 partial output and metadata word for sliding and full
attention, including incomplete key partitions.

## Full first-request comparisons

RTX 4070, AD-IQ4_XS, 32768 prompt and 512 output tokens, context 36864,
batch/ubatch 4096/4096, Q4_0 KV, and expert cache 6720 MiB. Both arms use
the aligned allocation. The four scored observations were made in A, B,
B, A order, with the reference and diagnostic traces taken before the
first B run. Each process evaluates the complete prompt.

| Arm | Prefill tok/s | Decode tok/s | Client s | Peak RSS KiB | Peak process VRAM MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A1 scalar Q4 | 432.20 | 31.39 | 92.411 | 13571948 | — |
| B1 half2 Q4 | 431.87 | 34.02 | 91.235 | 13571220 | — |
| B2 half2 Q4 | 431.88 | 33.98 | 91.244 | 13570636 | 9822 |
| A2 scalar Q4 | 433.19 | 31.22 | 92.340 | 13570640 | 9822 |

All four responses match the frozen EXP-2128 response in all 512 IDs,
text and stop type. Scored disk reads, process swap and system swap-out
were zero. Mean decode improved 8.61%, from 31.30 to 34.00 tok/s. Mean
prefill changed by −0.19%, from 432.69 to 431.87 tok/s. The fresh
EXP-2128 observation is 444.13 prefill and 36.93 decode tok/s. Restoring
that checkpoint and exceeding the frozen rates by at least 25% are
still open. A separate Apex compatibility attempt ended at the model RAM
residency gate before executing any request. This card verifies Inkling.

| Artifact | SHA-256 |
| --- | --- |
| Aligned scalar-Q4 executable | `c509ddbebc695bc9270908a792412bd3c8169b6c63f93c54751697805517238e` |
| Aligned half2-Q4 executable | `eb36020f88e0701e93c55c0ae9f0574d1d9d9598e899cec137623d55807151d3` |
| Half2 native CUDA image | `3c1771fb1844c0c64844f8c34c3665ce6abfbad2e6144be42defd3dfb398523c` |
| Inkling GGUF | `dbedbc18d04fc667df5c17772f0091cf21c763c896f504c088f976f19bbe75c0` |
| Frozen request | `a0e3e565f497045383aca3d431252780990c28498041a01de7ce2176c028f99f` |
| A1 response | `1c62b21b51e1983748cf2102305b8d0f2f6c30e15f99e1baab57ecaaf1675dbc` |
| B1 response | `2de4db1ee601ad8877c7ad2e497cfc7ea545c4c59ea7a2dc19a0153ae3407b70` |
| B2 response | `9b4ef87e3fbf86fce9d0ea60593bc628b2866128e452897a17adb87818716e8b` |
| A2 response | `c6d4009247a0f0bdd375e63cf974226ad659256a5efa5b461c99000a3709f6` |
