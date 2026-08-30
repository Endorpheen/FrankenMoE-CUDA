# FrankenMoE-CUDA

FrankenMoE-CUDA is an experimental, lossless runtime for Mixture-of-Experts models that are larger than both system RAM and GPU VRAM.

It combines multi-shard GGUF expert streaming from [BigMoeOnEdge](https://github.com/Helldez/BigMoeOnEdge) with the bounded CUDA expert hot store from the [`expert-tier`](https://github.com/01554/llama.cpp/tree/expert-tier) branch of `llama.cpp`.

```text
NVMe GGUF -> bounded RAM LRU -> pinned staging -> bounded VRAM hot store -> CUDA
```

## What has been demonstrated

Qwen3.8-Flash-Next `UD-IQ3_XXS` was run end to end from three GGUF shards totaling 81.96 GB on a desktop with:

- AMD Ryzen 9 5950X
- 64 GB DDR4
- NVIDIA RTX 4070 12 GB
- a consumer NVMe SSD
- Ubuntu 26.04

The model produced coherent text and code while the full model remained non-resident. Process swap stayed at zero in the recorded runs.

| Run | Result |
| --- | ---: |
| Cold, 128 tokens | 7.25 tok/s |
| Warm profile, 255 tokens until EOG | 8.96 tok/s average |
| First 32 warm-profile tokens | 174 ms/token |
| Last 32 warm-profile tokens | 85 ms/token |
| Warm RAM-cache hit rate | 91.5% |
| Warm SSD traffic | 19.9 MiB/token in the last window |
| Peak process RSS | 25.0 GiB |
| Process swap | 0 MiB |
| PLE table | 27.3 GiB served through a 64 MiB row window |
| CUDA hot store | about 2 GiB under concurrent desktop GPU use |

The interactive `llama-server` path has also been observed at roughly **14–15 tok/s after warm-up** in real coding conversations. That number is an interactive observation, not the same instrumented workload as the JSON benchmarks above.

Machine-readable results are in [`results/`](results/). Hardware, prompts, cache state, background GPU use, and storage performance materially affect throughput.

## Runtime properties

- Opens a split GGUF through its first shard; no file concatenation is required.
- Reads only experts selected by the original top-k router.
- Keeps cold experts in a configurable RAM cache rather than loading the complete expert set.
- Promotes hot experts through pinned host staging into a bounded VRAM bank.
- Keeps dense and non-MoE weights under the normal `llama.cpp` scheduler.
- Streams the large Qwen PLE table by addressed row reads.
- Prevents RAM eviction while an expert is being read or used.
- Publishes a new VRAM slot mapping only after a successful transfer.
- Exposes SSD, RAM-cache, H2D, VRAM, RSS, swap, and temperature telemetry.
- Leaves expert dropping, substitution, route changes, and reduced top-k disabled by default.

The small-model gate compares the streamed and resident greedy byte streams. It also exercises split GGUF indexing, RAM eviction, VRAM slot replacement, overlap, and clean shutdown.

## Build

Requirements include Git, CMake, Ninja, a C++ compiler compatible with the installed CUDA toolkit, and an NVIDIA GPU. The verified machine used GCC 13, CUDA Toolkit 12.4, and `sm_89`.

```bash
git clone https://github.com/Endorpheen/FrankenMoE-CUDA.git
cd FrankenMoE-CUDA
scripts/build.sh
scripts/test_small.sh
```

The build script clones pinned upstream revisions into ignored `upstream/` and `work/` directories, applies [`patches/expert-tier-integration.patch`](patches/expert-tier-integration.patch), and builds `build/franken-cuda/cli/bmoe-cli`.

See [`docs/BUILD.md`](docs/BUILD.md) for configuration variables and verified tool versions.

## Run the full Qwen model

Download all three `UD-IQ3_XXS` shards:

```bash
hf download unsloth/Qwen3.8-Flash-Next-GGUF \
  --include 'UD-IQ3_XXS/*.gguf' \
  --local-dir models/qwen38
```

Run the instrumented lossless path:

```bash
scripts/run_qwen38.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf
```

Run the interactive web UI and OpenAI-compatible server:

```bash
scripts/run_qwen38_server.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf
```

Then open `http://127.0.0.1:8080`.

Read [`docs/RUN_QWEN38.md`](docs/RUN_QWEN38.md) before changing memory budgets.

## Status and next step

The end-to-end prototype works. The primary remaining performance target is true H2D/compute overlap. The current pinned staging path is safe, but the public `ggml_backend_tensor_set` call synchronizes the transfer before returning. A future implementation needs explicit CUDA stream/event ownership so one expert can compute while the next promotion is in flight, without allowing either RAM or VRAM eviction too early.

This is research software. Node names and evaluation callbacks used by the integration are not stable `llama.cpp` APIs. See [`docs/LIMITATIONS.md`](docs/LIMITATIONS.md).

## Attribution and license

The runtime derives from BigMoeOnEdge and retains its Apache-2.0 license. The `llama.cpp` integration is distributed as a patch against the pinned `expert-tier` revision, preserving upstream attribution and licenses. OMLX code was not copied; its SSD expert-streaming work informed architectural choices only.

- [Pinned upstream revisions](UPSTREAMS.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Experiment record](docs/EXPERIMENTS.md)
- [Apache-2.0 license](LICENSE)
