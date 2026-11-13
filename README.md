# FrankenMoE-CUDA

FrankenMoE-CUDA is an experimental project for running Mixture-of-Experts models that are larger than both system RAM and GPU VRAM.

## Goose migration

The runtime is being ported to Goose, the project's own language. All 4061 Goose
sources are in one place: [`goose/`](goose/README.md), grouped by subsystem
(`jinja/`, `json/`, `server/`, `cpu/`, `cuda/`, `kv/`, `model/`, ...). A complete
module is one file named after the native module it replaces
(`jinja/lexer.goose` for `common/jinja/lexer.cpp`); partial slices are named
after the decision they implement. [`GOOSE.md`](GOOSE.md) is the short map and
`goose/MANIFEST.json` records the experiment, native path, gate and SHA256 of
every file. The current project state is
[`docs/goose-migration/STATUS.md`](docs/goose-migration/STATUS.md).

The results below predate the migration and describe the C++ runtime; the root
README is queued for a rewrite.

The repository currently contains two distinct runtime paths:

- a standalone research runtime derived from [BigMoeOnEdge](https://github.com/Helldez/BigMoeOnEdge), available as `bmoe-cli`;
- an interactive `llama.cpp` server: the current Apex/Flint bundle uses the pinned upstream revision documented below; the preserved Qwen lineage uses the public [`expert-tier`](https://github.com/01554/llama.cpp/tree/expert-tier) branch with this repository's patches.

The public `expert-tier` fork supplies the fast CPU-MoE execution path used by the earlier Qwen server. FrankenMoE retains upstream attribution: improvements are measured against explicitly identified controls, with separate results for the newer Apex/Flint bundle.

The standalone streaming path is:

```text
NVMe GGUF -> bounded RAM LRU -> pinned staging -> bounded VRAM hot store -> CUDA
```

## Latest validated results — October 30, 2026

The current accepted Apex/Flint runtime bundle is **EXP090**, retaining the
EXP087 memory optimization and EXP089 prefill optimization. The measurements
below use **Apex-35B-V1.0 Q4_K_M** on a Ryzen 9 5950X, 64 GB RAM and an RTX 4070
12 GB: configured context 262,144, actual prompt 32,768 tokens, CPU MoE,
batch/ubatch 4,096, Q4_0 KV, Flash Attention, one slot, no speculation and EHS off.
Each experiment uses five balanced serial control/candidate pairs.


### Models and validation scope

| Model / exact quantization | Maintained runtime | Validated scope |
| --- | --- | --- |
| **Apex-35B-V1.0 Q4_K_M** (`Apex-35B-V1.0-Q4_K_M.gguf`, hybrid `qwen35moe`) | EXP090 | EXP087 VRAM −30.86% and EXP089 prefill +61.25% on the 32K workload below; prefix/branch/restore and near-capacity correctness checks |
| **Flint INT4 Q4_1** (`flint-int4-q4_1.gguf`, `flint` with SWA) | EXP090-compatible bundle | Same generic optimizations retained; cold32K, shared-prefix branch, repeat and early-branch functional checks pass; **no paired Flint speedup or 30% memory claim** |
| **Qwen3.8-Flash-Next UD-IQ3_XXS**, three shards / 81.96 GB | Preserved EXP063 Qwen lineage | Earlier CPU-MoE, split MTP and pinned-upload-ring results; EXP062 three-slot ring improves prefill by median paired 11.537% over two slots; separate workload and build |

The latest registration/reservation changes are not a tested migration of the
older Qwen profile into EXP090. Models, quantizations and unsupported modes
outside this matrix have no claimed performance result.

### Exact Apex measurement conditions

- Hardware: Ryzen 9 5950X, 64 GB installed DDR4 (about 60.7 GiB usable), RTX 4070
  12 GB / `sm_89`, Linux desktop with concurrent desktop GPU allocations. VRAM
  figures refer to the model process, not total GPU occupancy or startup peak.
- Placement: `-ngl all --cpu-moe -fit off --load-mode mmap`; all 40 MoE expert
  layers on CPU, GPU attention/dense work, resident file-backed expert pages.
  This differs from the older Apex profile with five expert layers on GPU.
- Capacity and batching: `-c 262144 -b 4096 -ub 4096 -np 1 -fa on
  -ctk q4_0 -ctv q4_0`; no reduction of configured context, KV precision or
  router top-k for these gains.
- CPU: `-t 8 -tb 16 --cpu-mask 0f0f --cpu-mask-batch ffff --cpu-strict 1
  --cpu-strict-batch 1 --poll 0`. No competing model, build or benchmark in the
  measurement windows.
- Prefix policy: `--cache-ram 0 --ctx-checkpoints 32`, existing bounded recurrent
  prefix checkpoints retained; each measured arm begins with an empty prefix
  cache. `--spec-type none`; EHS off; Jinja enabled in the server profile.
- Request: native streaming `/completion`, identical list of 32,768 input token
  IDs, `n_predict=128`, `seed=123`, temperature 1.0, top-p 0.95, top-k 20,
  min-p 0, `cache_prompt=true`, `ignore_eos=false`, returned tokens and top-5
  probabilities. This is an inference benchmark, not an agent-task benchmark.
- Cache/ordering: warm model-file cache, fresh server for each arm with normal
  startup warmup; five manual serial pairs ordered AB, BA, AB, BA, AB. Cold
  here means an empty prompt cache, **not** a cold SSD/page-cache start.
  Qualification, profiled runs and other functional requests are excluded.
- Metrics: client TTFT ends at the first generated token; prefill throughput
  counts actually computed input tokens; decode is one response. Startup is
  reported separately. EXP087 VRAM is a 100 ms NVML-backed sampled request-window
  process peak; EXP089 VRAM is a separate qualification observation. Request
  resource samples check process swap, system swap-out and major faults.
- Model identity: SHA-256
  `c3b92d94cd03a7730ad45cc45de8c753e369e0228b2f5d83d819ce2ea6ab3355`.
  Source base: upstream llama.cpp
  `1b44e30775dcb2f93952116b05f89e5bd797fb00`, followed in order by
  `flint-apex-prefix-baseline.patch`, `lazy-pp-reservation.patch`,
  `mmap-expert-registration.patch`, `mmap-expert-available-memory.patch`.
  Use the linked source provenance and nine-file runtime manifests rather than
  the executable version string alone.

Flint compatibility uses context 131,072, batch/ubatch 4,096, CPU MoE, threads
8/16, Q4_0 KV, one slot, mmap, FA and no speculation; its profile also sets
`--reasoning-budget 4096`. File faults during those model-switching checks mean
their timings are not accepted performance samples. Exact per-model commands
are in the EXP090 launch profiles linked below.

| Accepted improvement | Control | Candidate | Median paired change |
| --- | ---: | ---: | ---: |
| EXP087: sampled request-window process VRAM peak | 6,404 MiB (EXP083) | **4,428 MiB** | **−30.86% / −1,976 MiB** |
| EXP089: prefill throughput | 1,394.259 tok/s (EXP087) | **2,257.972 tok/s** | **+61.25%** |
| EXP089: client time to first generated token | 23.512496 s (EXP087) | **14.522125 s** | **−37.97%** |

These compare successive preserved versions of **our fork**, not current stock
llama.cpp or every model. Percentages are medians of paired changes; they need
not equal the ratio of the displayed arm medians. EXP087 also reduced RSS by
6.62%, with prefill elapsed +1.16% and single-response decode +3.40%. EXP089's
single-response decode change was −1.13%; startup took about 1.46 s longer.
The memory and prefill improvements coexist; their percentages are different
metrics and must not be added.

**Why it works:** EXP087 stops physically allocating prompt compute buffers for
the worst-case full context at startup. It keeps the configured capacity and
allocates for the graph actually needed, with bounded host-buffer headroom.
EXP089 registers already resident mmap expert-weight pages for direct GPU
transfers, avoiding pageable staging without making another weight copy or
changing the model's arithmetic. EXP090 removes an overly restrictive raw-free-RAM
veto while retaining available-memory, residency, capacity and cleanup checks.

**Correctness and limits:** each accepted five-pair experiment matched all 640
output token IDs per arm, ordered top-5 IDs and probabilities exactly (maximum
delta 0). Primary request windows had process swap 0, no system swap-out growth
and no major faults. Apex prefix/branch/restore and near-capacity checks, plus
Flint functional compatibility checks, passed. These checks establish the tested
paths; they are not an all-model performance or semantic-quality benchmark.
Larger graphs can grow and retain buffers, so 30.86% is a 32K-workload result.
Resident mmap registration has supported-mode and host-memory gates with the old
path as fallback. EXP089 qualification observed 4,428 MiB after both variants;
it did not collect another five-pair VRAM-peak series.

Local Apex assisted with implementation and diagnostic tooling; the accepted
results come from the recorded correctness gates and paired measurements.

**Use the right bundle:** the local EXP090 installation is
`/home/end0/AI/LlamaServer/exp090-mmap-available/build/bin`. Keep its executable
and shared libraries together and set `LD_LIBRARY_PATH` to that same directory.
The legacy Qwen launcher `scripts/run_qwen38_server.sh` still defaults to
`build/exp063-default-runtime`; it does **not** select EXP090 automatically.
The old Qwen builds remain preserved. Experimental successors are not accepted
baselines until their own checks and measurements pass.

Source patches, exact runtime hashes, launch profiles, raw-evidence hashes and
measurement details:

- [EXP087 memory results and checks](results/exp087-lazy-reserve/README.md)
- [EXP089 prefill results and limits](results/exp089-mmap-register/README.md)
- [EXP090 delivery and activation](results/exp090-mmap-available/README.md)
- [Current runtime manifest](results/exp090-mmap-available/runtime-manifest.json)
- [Current launch profiles](results/exp090-mmap-available/launch-profiles.json)

The following Qwen and standalone sections preserve earlier results; they are
separate workloads and are not the latest Apex benchmark or launch profile.

## What has been demonstrated

Qwen3.8-Flash-Next `UD-IQ3_XXS` was run end to end from three GGUF shards totaling 81.96 GB on a desktop with:

- AMD Ryzen 9 5950X
- 64 GB DDR4
- NVIDIA RTX 4070 12 GB
- a consumer NVMe SSD
- Ubuntu 26.04

The model produced coherent text and code while the complete model remained non-resident. Process swap stayed at zero in the recorded runs.

### Earlier Qwen interactive server

The earlier validated Qwen profile runs the 81.96 GB model with a configured context capacity of **196,608 tokens** on the 12 GB RTX 4070. This was the server configuration used for these measurements; it is not a claim that a full 196,608-token prompt has already been benchmarked end to end.

| Current server result | Control | FrankenMoE candidate | Result |
| --- | ---: | ---: | ---: |
| 395-token bulk prefill latency | 3990.943 ms | **3465.613 ms** | **13.16% faster** |
| Decode throughput in the same A/B runs | 21.061 tok/s | **22.163 tok/s** | **5.23% higher** |
| Paired prefill wins | — | **5 / 5** | PASS |
| Exact paired bootstrap interval | — | **9.36% to 13.80%** | Above zero |
| Output correctness | Reference | Byte-identical | PASS |
| Process swap | 0 MiB | 0 MiB | PASS |
| Additional pinned-host budget | 0 MiB | 32 MiB | Within budget |

These EXP-038 results use the same fixed prompt, 48 generated tokens, one fresh server per run, 12 CPU threads, EHS disabled, split MTP with `--spec-draft-n-max 2`, and `-c 196608`. The candidate enables a two-slot pinned-host ring that overlaps CPU preparation with direct expert-weight uploads. All five candidate runs confirmed 17,886 staged transfers. The optimization targets bulk prefill; the observed decode increase is encouraging and will be rechecked across broader workloads in R6. See the [experiment report](experiments/EXP-2026-09-06-038-bounded-pinned-ring.md) and [machine-readable results](benchmarks/exp038-bounded-pinned-ring.json).

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

### Server history and attribution

The earlier Qwen daily-use server is the public `expert-tier` fork with the FrankenMoE integration patch applied. On the verified warm 256-token workload, the accepted 12-thread profile measured **17.615 tok/s median**, compared with **17.656 tok/s** at 16 threads. The `-0.23%` difference is inside observed run-to-run noise, while mean occupied CPU cores fell from 15.93 to 11.95.

On top of that floor, EXP-2026-09-03-024 added a head-only MTP speculative draft (stock MTP head as a sidecar GGUF, dense part on GPU and experts on CPU via `--spec-draft-cpu-moe`). In the paired same-binary test the warm 256-token greedy workload rose from 18.58 to **21.10 tok/s (+13.6%)** at draft acceptance 0.71 and mean accepted chain 2.42, with 934 MiB of VRAM to spare at `-c 32000`. A fully-CPU draft placement was tested first and rejected (`-13.1%`, EXP-023) because draft and verify serialize on the same CPU threads.

Compared with the clean public fork, our patches have demonstrated:

- lower peak process RSS during model loading: approximately 29.8 GiB instead of 42.7 GiB;
- identical output in the validated clean-fork versus patched-fork comparison;
- process swap protection and automatic shutdown on unsafe swap growth;
- a fail-closed RAM-headroom check before model startup;
- bounded VRAM hot-store autofit with an explicit reserve;
- a bounded pinned-host upload ring that reduced median bulk-prefill latency by 13.16%;
- reproducible benchmark, correctness, CPU, GPU, memory, and storage telemetry.

The server's non-speculative 14–18 tok/s range primarily comes from upstream `expert-tier`; FrankenMoE uses it as the performance floor. The accepted MTP draft configuration lifted the same server to 21.10 tok/s on its recorded warm workload, and EXP-038 measured 22.16 tok/s alongside the prefill improvement. Workload and protocol differences mean these figures should not be treated as one continuous benchmark series.

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

## Legacy Qwen server properties

- Uses the public `expert-tier` CPU-MoE implementation as its execution base.
- Avoids faulting an entire large GGUF shard into process RSS during startup.
- Defaults to 12 CPU threads; `THREADS=16` restores the previous maximum-CPU profile.
- Defaults to `EHS=0` because EXP-010 found that a one-slot GPU expert hot store reduced warm decode speed by 7.91% and produced unstable greedy hashes.
- Retains `EHS=-1` as an explicit experimental bounded-autofit mode.
- Loads a head-only MTP sidecar GGUF as a speculative draft: the head's dense tensors run on the GPU next to the target model while its experts stay on the CPU (`--spec-draft-cpu-moe`), so the draft fits in the remaining VRAM of a 12 GB card.
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

The following server recipes document historical Qwen builds. For the current
Apex/Flint bundle, use the EXP090 delivery and launch profiles linked above.

Build the legacy patched interactive server separately:

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

The historical EXP-2026-09-07-041 runtime is a reproducible chain over the pinned expert-tier
base `4aaad5d318a790a42c2197975ec8fadbad42602b` with four patches applied in order plus one
minimal default-on change:

```bash
git clone --no-hardlinks upstream/llama.cpp-expert-tier work/llama.cpp-exp041
git -C work/llama.cpp-exp041 checkout --detach 4aaad5d318a790a42c2197975ec8fadbad42602b
for p in expert-tier-integration integration-drift mtp-sidecar pinned-ring pinned-ring-default-on; do
    git -C work/llama.cpp-exp041 apply patches/$p.patch
done
cmake -S work/llama.cpp-exp041 -B build/exp041-default-runtime \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DGGML_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=89 -DLLAMA_CURL=OFF -DLLAMA_OPENSSL=ON -DLLAMA_BUILD_SERVER=ON
cmake --build build/exp041-default-runtime -j 10 -- llama-server
```

`patches/pinned-ring.patch` is the accepted EXP-038 ring (sha256
`e1e6803cec1e26099ee452fcb3537b7e660132e6e30da69febffca4785dbd7de`);
`patches/pinned-ring-default-on.patch` only flips the default of `GGML_EXPERT_PINNED_RING` from
off to on (`0` still disables the ring; allocation failure still falls back to the pageable path).

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

The preserved EXP041 direct-command example below uses its historical profile (pinned upload
ring on by default), the MTP speculative draft (requires the MTP head GGUF at
`models/qwen38/MTP/mtp-Qwen3.8-Flash-Next-Q4_K_M.gguf`; both embedding tables move to the CPU so
that the head fits on the GPU), context capacity 196,608 tokens, KV cache q4_0:

```bash
scripts/run_qwen38_server.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf
```

The equivalent direct command:

```bash
build/exp041-default-runtime/bin/llama-server \
  -m models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf \
  --host 127.0.0.1 --port 8081 \
  -c 196608 -np 1 -fa on --jinja -t 12 -ctk q4_0 -ctv q4_0 -ctkd q4_0 -ctvd q4_0 \
  --reasoning-effort low -ehs 0 --cpu-moe \
  -ot per_layer_token_embd.weight=CPU,token_embd.weight=CPU -ngl 99 \
  -md models/qwen38/MTP/mtp-Qwen3.8-Flash-Next-Q4_K_M.gguf \
  --spec-type draft-mtp --spec-draft-n-max 2 -ngld 99 --spec-draft-cpu-moe
```

To temporarily run without the pinned upload ring (diagnostics and control runs):

```bash
PINNED_RING=0 scripts/run_qwen38_server.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf
```

Do not replace the split placement with `-ngld 0`: a fully-CPU draft was measured 13.1% slower than no draft at all (EXP-023).

The launcher defaults to `THREADS=12`, `EHS=0`, `PINNED_RING=1`, `MTP=1`, context `196608`,
`-np 1`, and `BUILD_DIR=build/exp063-default-runtime` (accepted three-slot ring). Set
`BUILD_DIR=build/expert-tier-franken-cuda` to launch the older build without the ring. Do not
enable the GPU expert hot store expecting an automatic speedup: the tested one-slot configuration
was slower and remains rejected.

Read [`docs/RUN_QWEN38.md`](docs/RUN_QWEN38.md) before changing memory budgets.

## Status and next step

Both the standalone prototype and the patched interactive server work. The patched server is the current user-facing path because it is substantially faster; the standalone runtime remains the controlled research path for deeper streaming changes.

CPU profiling showed that IQ2_S and IQ4_NL vector-dot kernels account for 53.69% of sampled cycles and OpenMP spin/wait paths account for about 39.96%. Disabling OpenMP did not help: EXP-015 measured a 2.06% slowdown with only a 1.47% reduction in CPU occupancy, so that configuration was rejected. Subsequent experiments ruled out several CPU-prefetch and single-buffer staging approaches. The accepted wins are the balanced 12-thread default (EXP-013), the split MTP speculative draft (EXP-024, +13.6% paired), and the bounded pinned-host upload ring (EXP-038, +13.16% median bulk-prefill improvement across five pairs).

The accepted MTP profile stays at `--spec-draft-n-max 2`. A larger draft limit was already checked outside the recorded benchmark protocol and is not scheduled for retesting; it is not part of the current roadmap.

The later Qwen EXP062/063 delivery increased the default pinned ring to three
slots: five paired runs reduced prefill time by a median paired 11.537% above
the two-slot control, at an additional 16 MiB pinned-host cost. The legacy
`scripts/run_qwen38_server.sh` selects `build/exp063-default-runtime`, with the
ring on by default and `PINNED_RING=0` available to disable it. See the
[EXP062 report](experiments/EXP-2026-09-15-062-ring-depth3.md). The newer Apex/Flint
EXP090 bundle and its independent measurements are documented at the top.

This is research software. Node names and evaluation callbacks used by the integration are not stable `llama.cpp` APIs. See [`docs/LIMITATIONS.md`](docs/LIMITATIONS.md).

## Attribution and license

The standalone runtime derives from BigMoeOnEdge and retains its Apache-2.0 license. The interactive `llama.cpp` integration is distributed as a patch against the pinned `expert-tier` revision, preserving upstream attribution and licenses. OMLX code was not copied; its SSD expert-streaming work informed architectural choices only.

- [Pinned upstream revisions](UPSTREAMS.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Initial experiment record](docs/EXPERIMENTS.md)
- [Ongoing experiment journal](experiments/EXPERIMENTS.md)
- [Apache-2.0 license](LICENSE)
