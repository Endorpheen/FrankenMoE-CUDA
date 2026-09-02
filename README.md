# FrankenMoE-CUDA

FrankenMoE-CUDA is an experimental project for running Mixture-of-Experts models that are larger than both system RAM and GPU VRAM.

The repository currently contains two distinct runtime paths:

- a standalone research runtime derived from [BigMoeOnEdge](https://github.com/Helldez/BigMoeOnEdge), available as `bmoe-cli`;
- the current interactive server: the public [`expert-tier`](https://github.com/01554/llama.cpp/tree/expert-tier) branch of `llama.cpp` plus the memory-safety, startup, and measurement patches maintained in this repository.

The public `expert-tier` fork supplies the fast CPU-MoE execution path used by the interactive server. FrankenMoE does not claim that upstream performance as an original speedup. The current work starts from that faster implementation and improves its memory control, safety, reproducibility, and eventually its throughput.

The standalone streaming path is:

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

The model produced coherent text and code while the complete model remained non-resident. Process swap stayed at zero in the recorded runs.

### Standalone research runtime

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

These figures belong to the standalone `bmoe-cli` streaming runtime. They must not be compared directly with the server results below because the workloads and implementations differ.

### Patched expert-tier server

The current daily-use server is the public `expert-tier` fork with the FrankenMoE integration patch applied. On the verified warm 256-token workload, the accepted 12-thread profile measured **17.615 tok/s median**, compared with **17.656 tok/s** at 16 threads. The `-0.23%` difference is inside observed run-to-run noise, while mean occupied CPU cores fell from 15.93 to 11.95.

Compared with the clean public fork, our patches have demonstrated:

- lower peak process RSS during model loading: approximately 29.8 GiB instead of 42.7 GiB;
- identical output in the validated clean-fork versus patched-fork comparison;
- process swap protection and automatic shutdown on unsafe swap growth;
- a fail-closed RAM-headroom check before model startup;
- bounded VRAM hot-store autofit with an explicit reserve;
- reproducible benchmark, correctness, CPU, GPU, memory, and storage telemetry.

The patches have not yet demonstrated a decode-speed improvement over the clean public fork. The server's current 14–18 tok/s range primarily comes from upstream `expert-tier`; FrankenMoE uses it as the performance floor for subsequent optimization.

Machine-readable results are in [`results/`](results/). Hardware, prompts, cache state, background GPU use, and storage performance materially affect throughput.

## Standalone runtime properties

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

## Patched server properties

- Uses the public `expert-tier` CPU-MoE implementation as its execution base.
- Avoids faulting an entire large GGUF shard into process RSS during startup.
- Defaults to 12 CPU threads; `THREADS=16` restores the previous maximum-CPU profile.
- Defaults to `EHS=0` because EXP-010 found that a one-slot GPU expert hot store reduced warm decode speed by 7.91% and produced unstable greedy hashes.
- Retains `EHS=-1` as an explicit experimental bounded-autofit mode.
- Reserves configurable RAM and VRAM headroom and monitors process swap.
- Exposes an OpenAI-compatible API through `llama-server`.

## Build

Requirements include Git, CMake, Ninja, a C++ compiler compatible with the installed CUDA toolkit, and an NVIDIA GPU. The verified machine used GCC 13, CUDA Toolkit 12.4, and `sm_89`.

```bash
git clone https://github.com/Endorpheen/FrankenMoE-CUDA.git
cd FrankenMoE-CUDA
scripts/build.sh
scripts/test_small.sh
```

These commands build and test the standalone runtime at `build/franken-cuda/cli/bmoe-cli`.

Build the patched interactive server separately:

```bash
scripts/prepare_upstreams.sh
cmake -S work/llama.cpp-integration \
  -B build/expert-tier-franken-cuda \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DGGML_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=89 \
  -DGGML_OPENMP=ON \
  -DLLAMA_BUILD_SERVER=ON
cmake --build build/expert-tier-franken-cuda \
  --target llama-server \
  -j 8
```

This produces `build/expert-tier-franken-cuda/bin/llama-server`. Keeping the standalone and server build directories separate prevents accidental comparisons between different implementations.

See [`docs/BUILD.md`](docs/BUILD.md) for configuration variables and verified tool versions.

## Run the full Qwen model

Download all three `UD-IQ3_XXS` shards:

```bash
hf download unsloth/Qwen3.8-Flash-Next-GGUF \
  --include 'UD-IQ3_XXS/*.gguf' \
  --local-dir models/qwen38
```

Run the standalone instrumented streaming runtime:

```bash
scripts/run_qwen38.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf
```

Run the patched `expert-tier` web UI and OpenAI-compatible server:

```bash
scripts/run_qwen38_server.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf
```

Then open `http://127.0.0.1:8080`.

The launcher currently defaults to `THREADS=12` and `EHS=0`. These can be overridden explicitly through environment variables. Do not enable the GPU expert hot store expecting an automatic speedup: the tested one-slot configuration was slower and remains rejected.

Read [`docs/RUN_QWEN38.md`](docs/RUN_QWEN38.md) before changing memory budgets.

## Status and next step

Both the standalone prototype and the patched interactive server work. The patched server is the current user-facing path because it is substantially faster; the standalone runtime remains the controlled research path for deeper streaming changes.

CPU profiling showed that IQ2_S and IQ4_NL vector-dot kernels account for 53.69% of sampled cycles and OpenMP spin/wait paths account for about 39.96%. Disabling OpenMP did not help: EXP-015 measured a 2.06% slowdown with only a 1.47% reduction in CPU occupancy, so that configuration was rejected.

The next experiment, EXP-016, investigates whether `MUL_MAT_ID` distributes selected-expert work unevenly across CPU workers. Existing profiling data will be analyzed first; a model run will only be requested if the saved data cannot answer the question. If imbalance is confirmed, chunk scheduling will be changed and benchmarked as a separate experiment.

The broader P1 target remains overlap and reduced idle time across SSD, RAM, CPU, and GPU. Coalesced expert reads, router-stage prefetch, double buffering, explicit H2D timing, `io_uring`, and `O_DIRECT` will be tested as isolated hypotheses rather than introduced together.

This is research software. Node names and evaluation callbacks used by the integration are not stable `llama.cpp` APIs. See [`docs/LIMITATIONS.md`](docs/LIMITATIONS.md).

## Attribution and license

The standalone runtime derives from BigMoeOnEdge and retains its Apache-2.0 license. The interactive `llama.cpp` integration is distributed as a patch against the pinned `expert-tier` revision, preserving upstream attribution and licenses. OMLX code was not copied; its SSD expert-streaming work informed architectural choices only.

- [Pinned upstream revisions](UPSTREAMS.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Initial experiment record](docs/EXPERIMENTS.md)
- [Ongoing experiment journal](experiments/EXPERIMENTS.md)
- [Apache-2.0 license](LICENSE)
