# FrankenMoE roadmap

Every performance change is accepted only after reproducible correctness checks and at least five comparable runs against the current baseline.

- [x] P0: reproducible benchmark, output identity gate, telemetry, and baseline history.
- [ ] P1: overlap SSD -> RAM -> GPU transfers with expert computation.
- [ ] P1: benchmark aligned coalesced reads, pinned RAM, and double/triple buffering separately.
- [ ] P2: hot-expert cache across VRAM, RAM, and SSD.
- [ ] P3: router-driven expert prefetch.
- [ ] P4: addressed expert container with aligned contiguous blocks.
- [ ] P5: fused dequantization + GEMM tuned for Ada and single-token decode.
- [ ] P6: MTP, n-gram, draft-model, and combined speculative decoding.
- [ ] P7: automatic VRAM/RAM/SSD residency planning.
- [ ] P8: sensitivity-driven mixed expert quantization.
- [ ] P9: reversible REAP pruning with representative quality gates.
- [ ] P10: dynamic batching and aggregate throughput for multiple agents.
- [ ] P11: autotuning profiles and an OpenAI-compatible server.

The current critical path is P0 followed by true H2D/compute overlap. UI work, pruning, mixed quantization, and multi-agent throughput remain deferred.
