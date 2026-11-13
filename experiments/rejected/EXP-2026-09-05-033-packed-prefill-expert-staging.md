# EXP-2026-09-05-033 — Packed prefill expert staging

Status: REJECTED

## Hypothesis

Combining sparse selected-expert H2D copies into one pinned transfer per projection would reduce prefill transfer overhead.

## Implementation

The isolated implementation added an optional CUDA packed-upload-and-scatter backend method. The scheduler collected the same selected contiguous expert ranges that the normal path copies, packed them into a pinned host buffer, sent one H2D payload, and scattered the bytes into their original GPU offsets. Router IDs, quantization, and the `MUL_MAT_ID` tensor layout remained unchanged.

## Results

With a fixed 395-token prompt and deterministic 16-token response, the experimental arm took 6502.22 ms for prompt evaluation (60.75 tok/s). The unmodified control took 3979.34 ms (99.26 tok/s). The experimental arm was 63.40% slower.

The response content SHA-256 matched exactly: `24122785e34e88b37d241f7c4f1f303575ab88c7065d05cbc6c0f9a7c60085b7`. Both runs used 11 MTP draft tokens with 9 accepted. No CUDA errors occurred and VRAM returned to baseline after shutdown.

## Cause

One staging buffer was protected correctly with a GPU event, but each refill waited for the prior upload/scatter. This serialized CPU packing and H2D work rather than overlapping them.

## When to revisit

Only revisit with a bounded multi-buffer pinned ring, per-slot events, and a proof that the CPU can prepare slot N+1 while CUDA consumes slot N. Preserve the same fixed request and compare at least five paired runs before claiming an improvement.

## Patch

`EXP-2026-09-05-033-packed-prefill-expert-staging.patch`
