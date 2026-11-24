# EXP-2026-11-24-1893 — Reuse Apex expert mmap pinning for Inkling

**Diagnostic only.** Reuse the Apex registration path, its page alignment,
RAM headroom checks, residency check, range merge and transactional
registration. The planner skips Inkling's absent third expert plane.
The test unregisters successfully acquired ranges before unmapping GGUF.

RTX 4070, 24 input tokens, 17 greedy outputs / 16 decode intervals,
context 128. Both arms use the same two-plane SLRU, resident routers,
small-route staging and async transfer flags. The only scored
switch is `I2_INSTALL_PINS=0/1`. All 17 token IDs, 57 FP32 state/logit
fingerprints and SLRU lookup/eviction statistics match.

| Result | Pageable | 4 GiB pin budget |
| --- | ---: | ---: |
| Registered pages | 0 | 3.92 GiB |
| Mean prefill tok/s | 7.50 | 7.81 |
| Mean decode tok/s | 5.40 | 5.46 |
| Mean TTFT s | 3.80 | 5.92 |
| Mean peak RSS GiB | 8.75 | 10.29 |

Two measured pairs had zero system swap-out. Matched-shape diagnostic
traces record 2,018,525,184 bytes of expert DMA in decode in both arms,
with summed device transfer durations 128.98 → 109.19 ms
(15.65 → 18.49 GB/s). Expert prefill DMA 385.13 → 359.82 ms at the
same 6.526 GB. These durations are the sum of expert-copy timeline events,
not wall time or full PCIe traffic. The unchanged transfer volume is the
limitation. About 1.24 ms of decode expert DMA per token was saved
on this request, without establishing a large full-model improvement.

Full pin registered 48 ranges totaling 12.34 GiB, but that candidate
run had 541287 system swap-out pages and roughly eight seconds longer
TTFT. The apparent performance improvement in that pair is not qualified.
Do not enable full pin based on this test. The partial candidate remains
optional research given its startup cost and modest decode impact.

## Reproduce

Use the EXP-1892 diagnostic harness with the source patches and pinned
planner, the EXP-1890 driver/base archive and EXP-1888 CUDA module.
The staging patch replaces the EXP-1890 historical driver input;
the small-route selector is enabled equally for both arms. Compile
`pinned.goose` in a separate KeelWing standalone unit with initializer
defines `-Dgoose_init=i2_pins_init -Dgg_init_globals=i2_pins_globals`.
Keep EXP-1891 flags and set `I2_RESIDENT_ROUTERS=1`,
`I2_STAGE_DECODE_EXPERTS=1`, `GGML_CUDA_REGISTER_HOST=1`,
`GOOSE_EXPERT_PIN_MIB=4096`, `GOOSE_CUDA_ASYNC_PACKETS=1`,
`GOOSE_CUDA_EXPERT_OVERLAP=1`, `GOOSE_CUDA_EXPERT_STAGE_PAGEABLE=1`
and `GOOSE_CUDA_EXPERT_STAGE_CURRENT=1`. Link the EXP-1888 cache shim.
Use [results](results.json) for prompt IDs and artifacts, and
[DMA results](expert-dma.json) for the phase-level profiler counters.
