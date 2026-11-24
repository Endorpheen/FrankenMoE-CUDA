# EXP-2026-11-24-1861 — Full Apex A/B for causal Flash interior tiles

**INCONCLUSIVE for baseline promotion.** The prefill gain replicated in
both pairs with exact output. Every A/B arm saw system swap-out. The
separate default-build run had zero system swap-out and process swap.

## Apex 32K/512 on RTX 4070

Context 262144, batch and ubatch 4096, Q4_0 KV, expert cache 6290 MiB,
split 4/count 1. Each fresh server ran a warm-up and a scored request.
A and B used the same executable with only the isolated causal Flash
cubin changed. Each request evaluated 32768 prompt tokens with zero
prefix reuse.

| Arm | Prefill tok/s | Decode tok/s | Post-request VRAM MiB | System swap-out pages |
| --- | ---: | ---: | ---: | ---: |
| A1 | 3224.99 | 81.33 | 8776 | 41494 |
| B1 | 3388.86 | 81.29 | 8776 | 6164 |
| B2 | 3390.98 | 81.51 | 8776 | 2821 |
| A2 | 3211.72 | 83.12 | 8776 | 1801 |
| Embedded default | 3359.10 | 83.28 | 8776 | 0 |

The mean prefill moved from 3218.36 to 3389.92 tok/s (+5.33%, 515 ms).
Mean decode moved from 82.23 to 81.40 tok/s (-1.01%). All ten replies
matched all 512 generated IDs and text against EXP-1856, and all
post-request properties including SLRU counters matched byte-for-byte.
Peak RSS ranged from 21490016 to 21493472 KiB and process swap stayed
at zero. The reverse pair and embedded build sampled VRAM every
250 ms with peaks of 8776 MiB. The forward pair only recorded
post-request VRAM. [Raw run identifiers and values](results.json).

## Build and next gate

The standalone cubin and its Goose change are documented in
[EXP-1859](../EXP-2026-11-24-1859-flash-interior/README.md). The
[driver patch](source/enable-by-default.patch) loads the same cubin
from an embedded array on SM89, keeping the original kernel if the
overlay cannot load. Use `GOOSE_FLASH_INTERIOR=0` to select the
original entry in the same executable. The embedded build log
confirmed that the interior kernel was selected with the flag unset.

Generate `flash-interior-image.h` with the bytes of `full.cubin` as
the `static const unsigned char flash_interior_image[]` initializer.
Apply the driver patch to EXP-1856's frozen driver, compile with
GCC 13 and its headers, and replace only `cuda-driver.o` in its
archive. Link its original infer-main. The exact local source,
archive, cubin, executable, launch profile and checksums are in the
`codex-20261124-flash-interior-validated` checkpoint.

Repeat A-B-B-A in the embedded executable, with `GOOSE_FLASH_INTERIOR=0`
for A and unset for B. Reject an arm with system swap-out and use the
existing model, 262144 context, 6290 MiB cache, 32K/512 request, and
correctness and memory gates. EXP-1856 remains the accepted baseline.
