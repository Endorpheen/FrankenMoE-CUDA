# Where the decode time actually goes: bandwidth 21%, synchronisation 3.5%

The status quo said the expert cache is the dominant lever and VRAM is the limit. This measures what is
left once the cache is in place, so the next optimisation is aimed rather than guessed.

## The measured numbers

| quantity | value | how |
|---|---|---|
| expert size, Q4_K_M | **1.69 MiB** | 3 * 2048 (n_embd) * 512 (expert_ff) * 0.5625 B, dimensions read from the GGUF |
| cache capacity | 2958 experts of 10496 | 4992 MiB / 1.69 MiB |
| hit rate | **81.4%** | cache counters (EXP: CACHE-OBSERVABILITY) |
| upload per plan | **2.80 MiB** measured, 2.51 predicted | 1.49 misses * 1.69 MiB |
| H2D per token | **103 MiB** | 41 layers * 8 experts * 18.6% * 1.69 MiB |
| H2D bandwidth | **4.1 GiB/s = 21% of PCIe 4.0 x16** | at 41 tok/s |

The measured 2.80 MiB and the predicted 2.51 MiB agree within the rounding of the emit granularity, so the
upload volume is fully explained: it is misses, nothing redundant.

## The synchronisation, timed not estimated

`copy()` does a D2H of the selected expert ids followed by a full `ggml_backend_synchronize`, once per layer.
Instrumented over one 512-token run:

```
calls=61441  avg=7.2 us  max=103.8 us
```

**442 ms total over a 12.8 s run = 3.5% of wall time.** Removing it entirely would give 40 -> 41.4 tok/s as
an upper bound - and it cannot be removed: the ids are produced on the GPU and must reach the host before
the policy can run. The honest bound on this lever is therefore under 1.4 tok/s, and reaching it means
replacing the full synchronise with an event wait, not deleting the dependency.

## What this rules out

- **Bandwidth is not the limit.** 21% of the link at 41 tok/s leaves 4x headroom. Uploading fewer bytes
  would not speed decode up.
- **A redundant copy is not the limit.** Upload volume matches misses to within rounding.
- **The synchronise is measurable but small** - 3.5%, upper bound 1.4 tok/s.

So the remaining decode time is **compute plus the D2H dependency**, not transfer. Any further work on the
cache should aim at the hit rate (fewer misses = fewer synchronises and less upload), which is now a change
to one Goose module rather than to C++.

## Cost

One instrumented build, one 512-token run, the probe removed again; the clean rebuild was verified at 39.60
@512 with the reference hash, unchanged from before the measurement.
