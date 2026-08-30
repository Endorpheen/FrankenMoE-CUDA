# Experiment record

## Baseline validation

- Official llama.cpp CUDA: 62/62 tests.
- BigMoeOnEdge CPU: 12/12 tests.
- expert-tier CUDA: 62/62 tests.
- Integrated runtime: 13/13 CTest cases with and without an accessible RTX 4070.
- 64 greedy tokens from the streamed small model matched the resident CUDA byte stream across repeated runs.
- Forced two-slot VRAM churn completed with live H2D copies and slot replacements.
- Split-GGUF, RAM eviction, row-window eviction, overlap, and clean shutdown were exercised.

## Small-model modes

The synthetic model has four layers, eight F32 experts, and top-2 routing. Absolute throughput is only a smoke benchmark.

| Mode | Decode tok/s | SSD MiB | H2D MiB/token | RAM cache MiB | VRAM cache MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| Resident | 6032.672 | 0.0 | 0.000 | 0.0 | 0.0 |
| BigMoe CPU | 352.301 | 87.4 | 0.000 | 1.9 | 0.0 |
| expert-tier RAM to VRAM | 1213.533 | 0.0 | 0.015 | 0.0 | 7.5 |
| SSD to RAM to VRAM | 380.009 | 87.7 | 0.015 | 1.9 | 7.5 |

## Full Qwen result

Model: Qwen3.8-Flash-Next `UD-IQ3_XXS`, 81,961,823,936 bytes across three shards. Shape: 48 layers, 512 experts, top-10, and a 27,963 MiB PLE table.

| Metric | Cold 128-token run | Warm-profile run |
| --- | ---: | ---: |
| Generated tokens | 128 | 255 before EOG |
| Decode | 7.25 tok/s | 8.96 tok/s |
| SSD read | 10,572.6 MiB | 13,220.2 MiB |
| RAM cache | 17,678 MiB, 85.7% hit | 20,310 MiB, 91.5% hit |
| CUDA hot store | 2,174 MiB | 1,993 MiB |
| H2D | 380.8 MiB, 600 copies | 946.8 MiB, 1,500 copies |
| VRAM slot swaps | 200 | 500 |
| PLE row reads | 45.9 MiB | 85.8 MiB |
| Peak RSS | 22.6 GiB | 25.0 GiB |
| Process swap | 0 MiB | 0 MiB |

Warm-profile windows:

| Window | Latency | SSD traffic | RAM hit rate |
| --- | ---: | ---: | ---: |
| First 32 tokens | 174 ms/token | 134.2 MiB/token | 49.9% |
| Tokens 64–96 | 120 ms/token | 58.6 MiB/token | 80.7% |
| Tokens 160–192 | 88 ms/token | 25.1 MiB/token | 88.8% |
| Last 32 tokens | 85 ms/token | 19.9 MiB/token | 91.1% |

The warm profile improved latency by 2.05x and reduced SSD traffic by 6.7x from the first to the last window. The model produced coherent natural language. Subsequent warmed interactive coding chats through `llama-server` were observed at approximately 14–15 tok/s.

Raw CSV traces are intentionally ignored because of their size. Summary JSON files are committed under `results/`.
