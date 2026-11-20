# Paired A/B at 32K input, four axes, 2026-11-18

**Excluded from the Apex performance baseline.** All four saved responses
contain exactly 10,234 line feeds and no substantive text. The requests did
not save output token IDs or candidate probabilities. See the [Apex audit](../../docs/goose-migration/APEX-BASELINE.md#excluded-32k-sampler-series)
for the provenance and the qualified reference. The observations below are
retained as historical timing data.

The earlier card (`results/p610-export-1195`) reported two axes and said so. The harness now
records all four, and the prompt is a real 32K:

```
on-1  r1 prompt_n=32001 prefill=3042.07 decode=80.29 vram_mib=11048 rss_kib=21851640
on-1  r2 prompt_n=32001 prefill=3637.76 decode=83.94 vram_mib=11056 rss_kib=21860756
off-1 r1 prompt_n=32001 prefill=3073.67 decode=81.47 vram_mib=11048 rss_kib=21846716
off-1 r2 prompt_n=32001 prefill=3654.43 decode=84.21 vram_mib=11056 rss_kib=21855864
```

| axis | ON | OFF | delta |
|---|---|---|---|
| prefill tok/s | 3339.9 | 3364.1 | **-0.72%** |
| decode tok/s | 82.12 | 82.84 | **-0.88%** |
| VRAM MiB | 11048 / 11056 | 11048 / 11056 | **0** |
| RSS | 21,856,198 KiB | 21,851,290 KiB | **+0.02%** |

One pair of two repeats: enough to confirm the direction and the memory axes, not enough to
separate a sub-1% decode difference - and the earlier 7K card already showed the ranges overlap.
The -0.88% sits on the accepted -0.97% from the sampler stage, so the chain's cost is consistent
across both prompt lengths rather than being an artefact of one.

## Two harness defects fixed to get here

1. **Argument list overflow.** At 32K the request body exceeds what can be passed in `--data`:
   `curl: Слишком длинный список аргументов`. The body now goes through a request file, which is
   also the right shape for a body this size.
2. **Only `timings` was read.** VRAM and RSS are now sampled with each repeat, so the four-axis
   rule is satisfied by the harness rather than by hand.
