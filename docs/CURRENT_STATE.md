# Current state

## Source state

- Branch: `main`.
- Baseline source commit: `ed95e46c013bf915e2217a58f4b94e04936a9466`.
- The root worktree was clean before P0 work started.
- The ignored `work/llama.cpp-integration` tree contained pre-existing comment-only edits in eight integration files. They are preserved and excluded from root commits.

## Runtime architecture

FrankenMoE combines BigMoeOnEdge addressed GGUF streaming with the bounded CUDA expert hot store from the pinned llama.cpp `expert-tier` branch.

```text
split GGUF on NVMe -> positioned O_DIRECT reads -> bounded RAM LRU
                   -> pinned host staging -> bounded VRAM hot store -> CUDA
```

Dense and non-MoE weights use the normal llama.cpp scheduler. A separate 64 MiB row window streams the 27.3 GiB Qwen PLE table. The original top-k router remains unchanged in correctness runs.

Four blocking `pread` lanes can already overlap expert reads with CPU cold-expert computation. RAM entries have read/use leases, and VRAM mappings are published only after a complete upload. The remaining P1 boundary is synchronous `ggml_backend_tensor_set`: pinned staging exists, but H2D transfer does not overlap CUDA compute.

The expert-tier GPU hot store is currently disabled by default (`EHS=0`). EXP-010 measured one live slot at `-7.91%` median warm decode and found unstable greedy output hashes. Safe bounded autofit remains available only as an explicit experimental mode (`EHS=-1`); it uses exact CUDA layout sizing, allocation backoff, and a mandatory 1024 MiB runtime headroom.

## Verified target

- CPU: AMD Ryzen 9 5950X.
- RAM: 64 GB installed, about 60 GiB visible.
- GPU: NVIDIA GeForce RTX 4070 12 GB, Ada (`sm_89`).
- Storage: XPG GAMMIX S11 Pro NVMe, ext4.
- OS: Ubuntu 26.04 LTS, kernel 7.0.0-30-generic.
- Driver: 580.173.02.
- CUDA Toolkit: 12.4.131.
- Build toolchain: GCC/G++ 13, CMake 4.2.3, Ninja 1.13.2.

## Build and correctness

```bash
scripts/build.sh
scripts/test_small.sh
```

The small-model gate compares resident and streamed greedy byte streams. A full resident comparison of the 81.96 GB model is impossible on the target machine, so the large-model baseline also requires identical greedy output hashes across runs, bounded memory, zero process swap, and non-zero SSD/H2D telemetry.

## Baseline workload

```bash
scripts/run_baseline.sh
```

The harness performs five 256-token requests with a fresh process cache each time and records the first and last 32-token windows separately. Raw CSV, output, and logs stay local; `benchmarks/baseline.json` stores every per-run summary and medians.

The accepted P0 baseline has median 8.515 tok/s overall, 4.876 tok/s over the first 32-token window, and 11.446 tok/s over the last 32-token window. The warm-window standard deviation is 0.424 tok/s. Median prompt processing is 4.11 tok/s and median TTFT is 6.874 s. Median RSS peak is 26,794 MiB, median VRAM delta peak is 6,874 MiB, and process swap remains zero. All three model shard SHA-256 values were recomputed and matched `docs/RUN_QWEN38.md`.

Full-model greedy output is not byte-deterministic across independent CUDA processes. All five hashes are retained rather than collapsed into a false equality claim. Lossless correctness is therefore gated by the byte-identical resident/streamed synthetic model, while the full model is checked for coherent output, stable routing telemetry, bounded memory, zero process swap, and absence of I/O errors.
