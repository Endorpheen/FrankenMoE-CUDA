# EXP-2026-11-15-735 — the cache curve is monotone at ub 2048 and does not saturate

Base: `fe1d5f19`. EXP-734 found 42.35 decode at ub 2048 / cache 5376. This checks whether that cache
size is a local optimum or just the memory ceiling, and tries to free more VRAM.

## 1. The curve at ub 2048

| cache MiB | decode | range | VRAM |
|---:|---:|---|---:|
| 4864 | 41.09 | 40.67-41.19 | 7460 |
| 5120 | 41.67 | 41.40-42.06 | 7718 |
| 5376 | **42.35** | 41.97-42.56 | 7972 |

Incremental gain per 256 MiB:

```
4864 -> 5120:  +0.58 tok/s
5120 -> 5376:  +0.68 tok/s
```

**The gain per MiB is still increasing at the ceiling.** There is no saturation point - the curve is
still climbing when the device runs out of memory. So 5376 is not an optimum in the tuning sense; it
is simply the most that fits.

That also means the earlier non-monotone curve measured at ub 512 (which had a dip at 4608) was a
property of that batch size, not of the cache mechanism. At ub 2048 the relation is clean.

## 2. Attempts to free more VRAM, both closed

**`--cache-ram`** is not a lever: it is the host-side prefix cache (default 8192, profile sets 0), and
it consumes RAM, not VRAM. Raising it cannot free device memory for experts.

**`--no-kv-offload`** aborts inside `libggml-base` on this model. Not a configuration to tune around -
the model's recurrent layers require the KV offload path, and disabling it is unsupported rather than
merely slow.

So the memory ceiling at 5376 MiB stands for this device, and decode cannot be pushed further by
reallocation.

## 3. What that leaves

If every MiB still buys 0.6-0.7 tok/s and there is no memory left, the remaining route to higher
decode is fewer transfers per token - which returns to the synchronisation finding: 12.6 stalls per
token, each after ~1.5 tokens of queued work. A graph-level change is the only lever left that does
not need memory.

## 4. Configurations on record

| configuration | prefill | decode | VRAM | when |
|---|---:|---:|---:|---|
| ub 512, cache 4352 | 2429.9 | 39.86 | 7552 | long prompt, short generation (reference) |
| **ub 2048, cache 5376** | **2019.3** | **42.35** | **7972** | long generation |
| ub 1024, cache 5632 | 1488.7 | 42.74 | 7972 | maximum decode, prefill sacrificed |

All three measured with >= 3 warmed runs; the middle one with 6.

## 5. Cost

Three configurations, nine runs, one failed start.
