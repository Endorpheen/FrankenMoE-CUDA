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

## Interactive server

```bash
CTX_SIZE=8192 \
scripts/run_qwen38_server.sh \
  models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf
```

Open `http://127.0.0.1:8080`. The same endpoint exposes the standard OpenAI-compatible `llama-server` API. On the validated machine, warmed interactive conversations have been observed around 14–15 tok/s.

Do not treat cache sizes as universal defaults. Desktop GPU use, context length, driver allocations, and KV size change the safe number of hot slots.
