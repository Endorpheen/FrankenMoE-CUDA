# The MoE cache had no working observability; it does now, and the first numbers are a finding

Found while chasing a probe that never printed. The chase itself was more informative than the fix.

## What was wrong

`log_stats()` existed and was called only from the destructor. Two consequences:

1. **A server stopped by a signal never runs the destructor**, so the statistics from every measured run in
   this project were lost. `grep llama_moe_cache` over any run log returned nothing - I checked several.
2. **`LLAMA_LOG_INFO` from `llama-moe-cache.cpp` never reaches the log at all.** A probe that wrote the
   `plan()` call count to a file recorded **21k calls** for a 512-token request, while every
   `LLAMA_LOG_INFO` from the same file was absent. So the macro does not produce output from this
   translation unit, whatever the reason - and a line that cannot be seen is worse than none.

## The fix

Periodic statistics from the hot path, written with `fprintf(stderr) + fflush` rather than the macro, once
per 4096 plans:

```
moe_cache: plans=4096  small[h=25117 m=8865 up=16209.8MiB] large[h=0 m=0 up=0.0MiB] copied_from_cache=16596 (9970.0MiB)
moe_cache: plans=8192  small[h=52732 m=14018 up=25759.1MiB] large[h=0 m=0 up=0.0MiB] copied_from_cache=16596 (9970.0MiB)
moe_cache: plans=12288 small[h=81054 m=18464 up=34012.0MiB] large[h=0 m=0 up=0.0MiB] copied_from_cache=16596 (9970.0MiB)
```

## What the first numbers say

- **Hit rate 81.4%** (81054 / 99518). Reasonable, not perfect.
- **34.0 GiB uploaded over 12288 plans = 2.8 MiB per plan.** That is the expert upload made visible. With
  4992 MiB of cache and 81% hits, the remaining 18.6% of misses still move 2.8 MiB per plan.
- **`copied_from_cache` is frozen at 16596 after the first dump.** That counter only advances in
  `copy_experts`, the path large batches use to read experts out of the cache - so it ran during **prefill
  only**, and during decode every hit is served without that path. Two readings: either decode reads the
  bank directly (no copy needed), or the large-batch copy path is not what decode uses.
- `large[h=0 m=0]` confirms the small-batch bucket handles everything: decode's 8 ids never exceed the
  8*n_expert_used threshold. That matches the design.

## Why this matters more than the line itself

EXP-752 measured that removing the cache halves decode (41.04 -> 20.66) but could not say *why* the cache
helps: no hit rate, no upload volume, no split between prefill and decode. Those three numbers are now
available per run. Any future cache work - bigger slots, better admission, prefetch - can be judged on them
instead of on end-to-end decode alone.

## Cost

`fprintf` once per 4096 plans. Decode after the change: 39.61 @512 over three runs, against 39.0-40.7 for
the same configuration earlier today - inside the run-to-run spread, so no measurable cost.
