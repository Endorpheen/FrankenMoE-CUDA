# FrankenMoE-CUDA

FrankenMoE-CUDA runs Mixture-of-Experts models that do not fit in GPU memory on
one consumer GPU. The runtime is derived from `llama.cpp` and is being migrated,
module by module, to the **Goose** programming language. The reference model is
**Apex-35B-V1.0 Q4_K_M** on a Ryzen 9 5950X, 64 GB RAM and an RTX 4070 12 GB.

## Performance headline

**Accepted Apex reference (EXP906, 2026-11-16): 55.5176 tok/s decode and
2625.2066 tok/s prefill** on one RTX 4070 12 GB, Apex-35B-V1.0 Q4_K_M, fixed
32768-token prompt / 512 generated, `--cpu-moe --moe-cache-mib 4992`.
The exact binary set, every SHA256 and the launch flags are pinned in
[`docs/reference/apex-exp906/manifest.json`](docs/reference/apex-exp906/manifest.json);
`tools/verify-apex-reference.sh` refuses to measure against anything else.

**Maxwell-V1-Flash Q4_0 (our 21.42 GB quant): 65.92 tok/s decode and
1895.80 tok/s prefill** on the same GPU with a 2048-token prompt and 128
generated tokens, measured as the median of three warm runs. Details are in
[`experiments/EXP-2026-11-17-934-maxwell-real-weights/`](experiments/EXP-2026-11-17-934-maxwell-real-weights/README.md).

## Releases

**[exp906-runtime-v1.0](https://github.com/Endorpheen/FrankenMoE-CUDA/releases/tag/exp906-runtime-v1.0)
(Latest)** — byte copy of the accepted Apex EXP906 runtime: `llama-server`,
`libllama*.so.0`, the Goose components and the accepted CUDA library
(`17e79510f378…`). Asset `exp906-runtime-linux-x86_64.tar.gz`, 80,623,245 bytes,
SHA256 `05f7b1b75d6fc15b65543b29a4751ff5066d3da08fa04787d18a6499ec1ce18f`.
Weights are not included.

The current state and the open work are in
[`docs/goose-migration/STATUS.md`](docs/goose-migration/STATUS.md).

## Accepted Apex reference — EXP906

| item | value |
|---|---|
| runtime | `apex-independent-fast-20261116/run/` (byte copy, pinned by manifest) |
| RSS / VRAM | 21350.9 MiB / 11468 MiB |
| CUDA library | sha256 `17e79510f37817400c1b268e401d16eb2f392d1ef7f3da979d5ccf975cb32240` |
| answer hash | `c2ff42cc1219ea4dfe6d69f02e008d0f90c0fc56260736a0aafb82cec362924f` |

Any other build is a candidate. It is compared to this manifest under identical
flags, never treated as a reference.

## Maxwell-V1-Flash (Q4_0)

Independent 4-bit quantisation of `maxwell-model/Maxwell-V1-Flash`
(maxwell_moe: 45 layers, 256 experts, top-12 sigmoid router with expert bias,
4 mHC residual streams, block-sparse GQA) built by our own converter. Every
2-D weight is Q4_0 (ggml reference packing); 1-D tensors stay F32 and the mHC
projections F16. GGUF 21,418,677,984 bytes, sha256
`8d53f1e39d9b1940f2639b364a01c420aec99cbf26320f1fdeb8a662945c11f3`.

The runtime has been tested on real weights. See the
[Maxwell benchmark](experiments/EXP-2026-11-17-934-maxwell-real-weights/README.md)
and the [reasoning integration](experiments/EXP-2026-11-17-942-maxwell-ui-reasoning/README.md)
for tested configurations and limitations.

## Goose migration

Goose modules are grouped by subsystem under [`goose/`](goose/README.md) and
linked into the llama.cpp-derived runtime. [`goose/MANIFEST.json`](goose/MANIFEST.json)
records source provenance and SHA256 checksums. The
[migration ledger](docs/goose-migration/tasks.json) tracks coverage.

## Repository map

| path | content |
|---|---|
| `goose/` | the Goose sources (start here) |
| `docs/goose-migration/` | migration ledger, status, per-requirement coverage and closures |
| `experiments/` | one directory per slice: source, differential fixture, build, mutations |
| `runtime/` | local runtime artefacts and baselines |
| `scripts/` | launch, fit and maintenance scripts |
| `editors/goose-vscode/` | VS Code extension, TextMate grammar and the Linguist package |
| `patches/`, `benchmarks/`, `results/` | integration patches and measurement records |

## Measurement rules

1. Each performance result identifies the model, runtime and workload.
2. Correctness comes first: tokens and probabilities must match the accepted
   reference, and every claim states the content hash.
3. Single-response decode is never presented as aggregate throughput.
4. A paired comparison below 2% is not claimed as a change without repeated
   pairs and the raw data.
5. Rejected experiments stay in the record with their numbers.

## Build from source

The CUDA build uses the pinned llama.cpp fork. Goose modules are compiled
separately and linked into the runtime.

Prerequisites: Linux x86_64, GCC 13, CMake 3.24 or newer, Ninja, a CUDA toolkit
for the GPU build, and the pinned Goose compiler (`goose-2026.3-string`, SHA256
in `docs/goose-migration/COMPILER-2026-3-STRING.md`).

1. Prepare the pinned upstreams. The script clones the exact commits from
   [`UPSTREAMS.md`](UPSTREAMS.md) and checks that each checkout matches:

   ```
   scripts/prepare_upstreams.sh
   ```

2. Build the C++ runtime from the llama.cpp fork:

   ```
   cmake -S /home/end0/AI/LlamaServer/llama.cpp-flint-cuda -B build-cuda -G Ninja \
     -DCMAKE_BUILD_TYPE=Release -DGGML_CUDA=ON \
     -DCMAKE_C_COMPILER=gcc-13 -DCMAKE_CXX_COMPILER=g++-13 \
     -DCMAKE_CUDA_ARCHITECTURES=89
   cmake --build build-cuda -j8
   ```

3. Build and check the canonical Goose unit. Set `GOOSE` to the full path to
   the compiler executable. CI uses the public
   [Goose v2026.4 release](https://github.com/aardappel/goose/releases/tag/v2026.4)
   with its SHA256 pinned in [the workflow](.github/workflows/ci.yml).

   ```
   GOOSE=/path/to/goose bash tools/build-main-runtime.sh
   python3 tools/check-goose-tree.py
   ```

## Run the accepted Apex reference

Extract the [EXP906 release bundle](https://github.com/Endorpheen/FrankenMoE-CUDA/releases/tag/exp906-runtime-v1.0)
and follow `LAUNCH.txt`. The
[reference manifest](docs/reference/apex-exp906/manifest.json) pins the binaries,
launch flags and benchmark workload.

## Attribution and license

The runtime derives from `llama.cpp` and keeps upstream attribution and licenses.
The Apex, Qwen and Flint models are distributed under their own licenses. This
repository's own work is Apache-2.0; see [`LICENSE`](LICENSE).

- [Current status](docs/goose-migration/STATUS.md)
- [Migration ledger](docs/goose-migration/tasks.json)
- [Goose sources](goose/README.md)
- [Pinned upstreams](UPSTREAMS.md)
- [Experiment journal](experiments/EXPERIMENTS.md)
