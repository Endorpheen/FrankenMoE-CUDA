# EXP-1691 — Keep the first prefill in the ordinary cache

This experiment tests the cold-request cost of
[EXP-1690](../EXP-2026-11-22-1690-temporary-prefill-experts/README.md).
The temporary plane is reserved from initialization. Its use starts
after the driver observes a successful expert lookup during decode.
Until then, the first request uses the ordinary SLRU path.

Both arms use the same executable, 6,190 MiB SLRU, 210 MiB temporary
plane, 384 MiB activation arena and full expert host registration.
The only changed setting is `GOOSE_CUDA_PREFILL_TEMP_WARM_ONLY`.
Both requests are full Apex 32K/512 with batch/ubatch 4,096/4,096
and zero reused prompt tokens.

| Measure | Temporary plane immediately | After decode has warmed the cache |
|---|---:|---:|
| Cold prefill, tok/s | 2,604.99 | 2,683.43 |
| Cold decode, tok/s | 33.39 | 33.99 |
| Cold client, s | 28.001 | 27.369 |
| Warm prefill, tok/s | 2,987.00 | 2,996.54 |
| Warm decode, tok/s | 33.95 | 34.19 |
| Warm client, s | 26.136 | 25.995 |
| Peak process VRAM, MiB | 8,668 | 8,668 |
| Peak RSS, KiB | 21,489,940 | 21,490,372 |
| Process swap / system swap-out from launch | 0 / 0 | 0 / 0 |

Both requests match the other arm's content, all 512 IDs and all
probabilities. The protected first request reduces temporary
projections from 1,920 to 960 across the two requests. Both arms get
58,872 read-only resident planes once warm. Keep this checkpoint
for scaling tests and the prior clean 3,003.86 prefill peak separately.

Only `cuda-driver.o` is rebuilt from the preceding experiment.
Raw responses, logs, source and checksums are in
`build/exp1691-cold-aware-prefill`. Executable SHA-256:
`7311583c8a002a74c35d5a14f32796f23b5be4577e5523e39549f6f91bc15a90`.
