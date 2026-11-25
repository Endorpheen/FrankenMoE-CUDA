# EXP-2026-11-25-2064 — Register cap for the cumulative Bonsai PQ2 tile

**Not promoted.** Duplicate the EXP-2059 cached FP16 MMA-partial
PTX entry and cap only the duplicate to 80 registers with a
fixed 256-thread launch. The reference uses 94 registers. Both
use 27,648 B shared memory. The capped cubin reports a 16-byte
stack frame and 12 spill bytes each for stores and loads.
The CUDA residency calculator reports two versus three maximum
active blocks per multiprocessor. This is a resource bound,
not measured warp occupancy.

## RTX 4070 diagnostic component trace

Both entries match bitwise on four real matrix shapes, including
an activation zero row and the QKV tail. One trace sample per
kernel, with common activation packing added, in milliseconds.

| Matrix / tokens | EXP-2059, 94 registers | Cap at 80 |
| --- | ---: | ---: |
| QKV tail / 129 | 0.503 | 0.464 |
| QKV / 1024 | 1.774 | 1.722 |
| FFN up / 1024 | 3.028 | 3.395 |
| FFN down / 1024 | 3.277 | 3.130 |

Full FFN down saves 4.50%, below the preregistered 5% component
threshold. FFN up slows about 12%. The data do not support replacing
the main dispatch or an aggregate throughput claim. The conditional
FFN-down observation is retained for a future approach that reduces
register pressure without spilling. No model-level timing, memory,
quality or cross-architecture execution was measured. The accepted
and cumulative packages are unchanged.

## Reproduce

Use the EXP-2057 PTX from the EXP-2059 cumulative route and the saved GGUF inventory.

```bash
exp=experiments/EXP-2026-11-25-2064-bonsai-mmq-register-cap
out=build/bonsai-mmq-register-cap-2064
bash "$exp/source/build.sh" "$out" build/bonsai-fp16-acc-2057/pipeline.ptx
CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check.py" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration" \
  "$FRANKENMOE_CHECKPOINTS/bonsai-integration/inventory.json" "$out/pipeline.cubin"
```

[Measured samples and bitwise component results](data/component.json).
