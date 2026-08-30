# Results

The committed JSON files contain summary telemetry for the resident, CPU-streamed, CUDA-hot, combined tiered, cold full-model, and warm full-model runs.

Small-model baselines can be reproduced with:

```bash
N_PREDICT=64 scripts/benchmark.sh build/franken-cuda/tests/tiny-moe-qwen3moe.gguf
```

Token-level CSV and monitor traces are generated beside the JSON summaries but are excluded from Git because of their size.
