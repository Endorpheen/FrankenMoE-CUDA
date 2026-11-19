# Paired A/B on the export-fn build, 2026-11-18

Same binary, one decision switched (`GGML_GOOSE_SAMPLER_*`), alternating ON/OFF, three pairs of two
repeats, `-b 4096 -ub 4096 -ngl all --cpu-moe --moe-cache-mib 4992`, 512 predicted tokens.

| axis | ON (median) | OFF (median) | delta |
|---|---|---|---|
| prefill tok/s | 2746.6 | 2760.9 | **-0.52%** |
| decode tok/s | 46.32 | 47.12 | **-1.70%** |

Ranges:

```
ON  prefill 2532.9 2975.3 2385.4 2960.4 2497.4 3009.6
OFF prefill 2501.0 2977.8 2491.4 3008.0 2544.0 3018.6
ON  decode   45.91  47.08  45.89  45.89  46.73  46.73
OFF decode   47.24  47.27  46.75  47.11  47.14  46.41
```

The decode ranges **overlap** (ON reaches 47.08, OFF falls to 46.41), so this pair does not
separate the two configurations: per the project rule the honest reading is that both remain
candidates, and the -1.70% is not called a regression on this evidence alone. It is, however,
consistent with the earlier accepted -0.97%, so the chain's cost is real and in that range.

## What this measurement does not cover

- **VRAM and RAM are not captured** by this harness: it reads only `timings`. The four-axis rule
  asks for them, so this card is incomplete on two axes and says so rather than quoting a number
  from another run as if it belonged here.
- **Prompt length is 7001 tokens, not 32K.** That is what the existing paired script builds
  (`"word "*7000`); a 32K-input run is a separate, longer measurement.

Both gaps are the next thing to fix in the harness before this result is used for acceptance.
