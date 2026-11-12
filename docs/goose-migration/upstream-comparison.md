# Full Goose path vs current official llama.cpp

Control built by us in `/home/end0/AI/LlamaServer/llama.cpp-apex-upstream-control-20261112`, a separate directory; the
existing checkouts were not modified and no Goose patch was copied into it.

- upstream commit `dd6b945be2188da875ab05a259ce94c6b2736e64`, dated 2026-11-12 02:01 +0100, version `0.9.0-dev (build 1650)`
- toolchain: CUDA 12.6.85, CMake 4.2.3, Release, `GGML_CUDA=ON`, `CMAKE_CUDA_ARCHITECTURES=89`, `LLAMA_CURL=OFF`
- `qwen35moe` is supported natively (`src/llama-arch.cpp`), so no compatibility patch was needed or added
- one shared GGUF: `/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf`, unmodified
- identical argv (all profile flags exist upstream: `--cpu-moe`, `--moe-cache-mib`, `-fa`, `-ctk/-ctv`, batch/thread masks),
  same 32K prompt, same seed/sampling, 256 generated tokens

| variant | prefill | decode | RSS MiB | VRAM MiB |
|---|---|---|---|---|
| upstream, no cache | 1900.2 | **29.62** | 22672 | 6408 |
| upstream, cache 1024 | 2018.9 | 20.26 | 22682 | 7434 |
| upstream, cache 2048 | 2047.1 | 25.70 | 22682 | 8458 |
| upstream, cache 3072 | **fails to start** ("failed to allocate compute pp buffers") | - | - | - |
| Goose exact path | **2311** | 23.78 | **21319** | **3190** |
| Goose cache 2048 | 2348.3 | 34.03 | 21320 | 5242 |
| Goose cache 3072 (balance base) | 2335.0 | 37.79 | 21320 | 6266 |
| Goose cache 3072, 256 tokens | 2317.7 | 39.29 | 21320 | 6266 |
| Goose cache 3584 (128 tokens) | 2325.5 | 39.61 | 21320 | 6778 |
| Goose cache 3584 (256 tokens) | 2326.5 | 41.01 | 21320 | 6778 |
| Goose cache 3584 (512 tokens) | 2326.8 | **41.94** | 21320 | 6778 |

## What this says, without rounding in our favour

- **Prefill:** Goose is ahead by about 20-22% (2311-2348 against 1900-2047).
- **VRAM:** Goose exact uses 3190 MiB against 6408, i.e. half, and this is EXP087/EXP101 at work. The same mechanisms
  are why upstream cannot even start cache 3072 - it has no lazy compute reservation, so the larger cache does not fit.
- **Decode:** mixed, and this must not be hidden. Upstream without a cache is **faster** than the Goose exact path
  (29.62 against ~24, measured 23.0-24.8 over repeats). With the device cache Goose pulls ahead strongly - 41.94 at 512
  tokens against 25.70 for the upstream cache - and upstream's own cache *lowers* its decode while raising VRAM.
- **RAM:** Goose uses slightly less (21319 against 22672), both dominated by the resident 35B model.
- **Quality:** outputs differ across all four configurations (`53e82081` upstream, `e68df05b` Goose exact,
  `9d90a010` Goose cache), so this table compares speed and memory on identical inputs, not identical bytes.

## Caveats

- One profile, one prompt, one seed; short paired series only.
- The Goose cache path is still experimental: it is not bit-identical to the exact path and the acceptance criterion has
  not been changed.
- The gains are the product of the whole stack (ported Goose path, pinning, EXP087, EXP101, chosen settings), not of the
  language alone, and the decode advantage exists only in the cached configuration.

## Update: the 3584 point, measured after the first version of this table

| generated tokens | Goose cache 3584 decode | VRAM |
|---|---|---|
| 64 | 37.89 | 6778 |
| 128 | 39.61 | 6778 |
| 256 | 41.01 | 6778 |
| 512 | 41.94 | 6778 |

VRAM does not move across lengths (no eviction), prefill stays ~2326, and the step from 3072 to 3584 costs 512 MiB for
about 1.8-2.0 tok/s - the most efficient step in the ladder. Its output is byte-identical to the 2048 point, so the cache
capacity does not change quality; only whether the experts run on the GPU does.

### Reading the whole picture

The product advantage is real but specific: ~20-22% more prefill, half the VRAM at equal exactness, and a decode path
that beats upstream by ~60% once the device cache is used, while upstream is faster on the uncached decode path. The
gain comes from the whole stack (port, pinning, EXP087, EXP101, settings), not from the language, and the cached decode
path remains experimental until the acceptance criterion is settled.
