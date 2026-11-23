# EXP-2026-11-23-1819 — Smaller context for a bigger expert cache: refuted

Status: **REFUTED with numbers.** Baseline `37ac0f1d` unchanged
(prefill 3122.4, decode 78.225).

## The experiment the owner authorised

Free VRAM by reducing the context from 262144 to 131072 (KV stays q4_0, batch and
ubatch stay 4096) and spend it on the expert cache, then compare prefill.

## What the engine reported

```
ctx 131072, cache 8000 requested: available 7678156800 bytes after model allocations
ctx 131072, cache 7600 requested: available 7706861568 bytes
```

So the freed budget is 7.30 GiB of available VRAM for the cache, against the
6.29 GiB the fixed profile uses: the largest workable cache is about 7300 MiB,
not the 8000 the plan hoped for.

## What it measured

One 32768-token prompt, one predicted token, warm-up plus measured request, same
binary, same switch set as the baseline diagnostics:

| configuration | prompt_ms | tok/s | cache | fills | upload |
| --- | ---: | ---: | ---: | ---: | ---: |
| ctx 262144, cache 6290 (baseline) | 12312.1 | 2661.5 | 6287 MiB | 49 809 | 94.8 GB |
| ctx 262144, cache 6290 (repeat) | 12342.1 | 2655.0 | 6287 MiB | 49 809 | 94.8 GB |
| ctx 131072, cache 7300 | 12241.5 | 2676.8 | 7298 MiB | **98 833** | **188.1 GB** |
| ctx 131072, cache 6290 | 12208.5 | 2684.0 | 6287 MiB | **99 137** | **188.6 GB** |

Two findings:

1. The larger cache does not reduce misses: with 7300 MiB the engine performs
   twice the fills of the fixed profile. The control at the same context and the
   old cache shows the same doubling, so the cause is the **context size**, not
   the cache: a smaller context changes how the bank is partitioned
   (`expert cache 1904/1531 rebalance`) and the prefill working set ends up
   streamed twice.
2. Even with double the traffic the prompt only moves by +0.9 % (2661.5 -> 2684.0),
   which also confirms that the transfer volume is largely overlapped, not the
   binding constraint.

## Verdict

The profile-change axis is refuted: the best configuration it reaches is 2684
tok/s in the diagnostic protocol, about +1 % against the same protocol, while the
target needs +8 %. The fixed-profile baseline of 3122.4 stands, and its 3500
target is not reachable by trading context for cache on this card.

## Artefacts

`runs/ctx128c7300-020647`, `runs/ctx128c6290-020744`, `runs/probe-ctx128-8000`,
`runs/probe-ctx128-7600`; baseline diagnostics `runs/drvtime-235827`,
`runs/sw-base-212205`.
