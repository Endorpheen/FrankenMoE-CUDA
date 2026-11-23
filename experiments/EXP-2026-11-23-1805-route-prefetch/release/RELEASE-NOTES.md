## FrankenMoE Chat Alpha 0.12.0 — faster Pure Goose decode

This release moves the Linux and Windows chat hosts onto the cumulative Pure
Goose runtime from the Apex performance work (EXP-1722 through EXP-1805). The
0.11.2 chat, web UI and OpenCode-compatible API stay the same.
Download the archive for Linux x86-64 or Windows x64 and verify it against
SHA256SUMS. Model weights are distributed separately.

What changed:

- The runtime now includes decode graph replay with plan reuse, compact
  routing of the eight selected experts, an unrolled router softmax, split
  expert weight planes, batched pinned expert DMA and a one-touch SLRU expert
  cache.
- New in EXP-1805: the experts of the next layer are predicted from the
  current layer and copied to the GPU while the current layer computes. The
  model always uses its own routes, so the arithmetic is unchanged; only the
  copy timing changes.
- The accepted runtime profile is now the default. Earlier chat builds needed
  environment switches for these paths and ran without them. Any `GOOSE_*`
  variable you set still wins. `GOOSE_EXPERT_PREFETCH=0` disables the
  prefetch, and `GOOSE_RUNTIME_DEFAULTS=0` restores the bare defaults.

Measured on Linux, RTX 4070 12 GB, Apex-35B-V1.0-Q4_K_M, context 262144,
expert cache 6290 MiB, batch 4096, q4_0 KV cache, 8/16 threads. All figures
are for a single response, not aggregate throughput:

- 32,768-token prompt, 512 generated tokens, compared with the previous
  accepted checkpoint EXP-1786 in two A/B pairs: decode 79.85 tok/s
  against 76.91 tok/s, prefill about 3075 tok/s. All 512 token ids and the
  text are identical to the reference.
- Short chat request (34 prompt tokens, 384 generated tokens, temperature 0),
  same server flags: decode 87.19 tok/s with this build against 35.47 tok/s
  with the 0.11.2 binary. Peak VRAM was 8464 MiB against 8122 MiB, and host
  RSS was unchanged. The two builds take different kernel paths, so their
  sampled texts differ. Both are coherent.

This remains an Alpha release. Native Windows inference with NVIDIA requires
community testing. The Windows build passed the HTTP transport gate under
Wine: JSON, SSE, tool calls, recovery and error routes. A CPU-only start with
the real model under Wine loaded all 40 layers and served `/health` and
`/v1/models`. It finished prompt processing, but the run was stopped before
generation. Wine has no CUDA, so this check did not cover GPU inference.
