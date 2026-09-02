# Running Qwen3.8-Flash-Next UD-IQ3_XXS

## Model files

Repository: `unsloth/Qwen3.8-Flash-Next-GGUF`, directory `UD-IQ3_XXS`.

| Shard | Bytes | SHA-256 |
| --- | ---: | --- |
| `...-00001-of-00003.gguf` | 10,946,624 | `268f81fdedf3149a538f252308927a4d5d1f6e062c178568a51e3b519744f8a8` |
| `...-00002-of-00003.gguf` | 49,567,921,344 | `cfe600b236b88c7fad1613a5ca5e83b9f2beb63cbd44c32b2be50a44747c695f` |
| `...-00003-of-00003.gguf` | 32,382,955,968 | `f1912ba34c79427d2295a58dcb2b732b5931af5bef7a373c60557a57d9ee7250` |

```bash
hf download unsloth/Qwen3.8-Flash-Next-GGUF \
  --include 'UD-IQ3_XXS/*.gguf' \
  --local-dir models/qwen38
```

Verify all three checksums before the first run. Pass only the first shard to the runtime.

## Instrumented run

```bash
N_PREDICT=128 \
CTX_SIZE=4096 \
RAM_CACHE_CEIL_MB=32768 \
VRAM_CACHE_MB=6144 \
scripts/run_qwen38.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf
```

The script preserves 8 GiB of available system memory, reserves 4 GiB of free VRAM, clamps both caches, enables the PLE row window and SSD overlap, and writes token plus system telemetry under `results/`.

The swap watchdog stops the process if its own `VmSwap` grows rapidly or if system swap I/O remains high. It never kills unrelated processes.

The validated cold run generated 128 greedy tokens at 7.25 tok/s. The warm-profile request ended naturally at 255 tokens and averaged 8.96 tok/s.

## Deterministic workload A/B

Full-model greedy CUDA output is not byte-deterministic across processes, so two configurations being compared normally execute different expert routes and read different amounts of data. Capture one natural run once and force every later run through exactly its token and expert-route sequence:

```bash
# 1. Capture one workload (natural routing, greedy), same flags as the A/B arms:
bmoe-cli -m <model> -p "<prompt>" -n 256 <baseline flags> --workload-capture run.workload

# 2. Replay it under each configuration; only the flag under test differs:
bmoe-cli -m <model> -p "<prompt>" -n 256 <baseline flags> --io-threads 4 --workload-replay run.workload
bmoe-cli -m <model> -p "<prompt>" -n 256 <baseline flags> --io-threads 8 --workload-replay run.workload
```

Rules and guarantees:

- Replay requires the same tokenized prompt, architecture, layer count, and effective top-k, and consumes exactly the captured number of route records and output tokens. Any deviation — different prompt, changed top-k, edited or missing route records, a different graph shape — fails closed with a specific error before or during the run.
- `-n` must be at least the captured token count; a larger `-n` simply stops at the workload's end. The generated stream is byte-identical to the capture in every replay.
- Capture and replay are mutually exclusive, require `--moe-stream`, work in one-shot mode only (not with `--session`, `--ppl`, or `--ppl-list`), and do not support speculative decoding.
- The replay applies after every route modifier (substitute, route-ahead), so the storage and expert-compute workload is exactly the captured one; physical reads can still differ slightly through cache timing, which is part of the effect under test. Logical demand (`token_demand_MiB`) is the work-equality check.
- The workload file stores arch/layers/top-k, not a model hash; `benchmarks/baseline.json` pins the shard SHA-256 values, which is the model-identity gate for local A/Bs.
- Matched-trajectory overhead measured about 1.3%; both arms of an A/B replay the same workload and pay the same overhead, so relative comparisons are unaffected. Capture mode allocates as it goes and is not a benchmark mode.

## Interactive server

```bash
CTX_SIZE=8192 \
scripts/run_qwen38_server.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf
```

Open `http://127.0.0.1:8080`. The same endpoint exposes the standard OpenAI-compatible `llama-server` API. On the validated machine, warmed interactive conversations have been observed around 14–15 tok/s.

Do not treat cache sizes as universal defaults. Desktop GPU use, context length, driver allocations, and KV size change the safe number of hot slots.
