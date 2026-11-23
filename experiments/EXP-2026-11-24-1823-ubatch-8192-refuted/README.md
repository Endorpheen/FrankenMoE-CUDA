# EXP-2026-11-24-1823 — Ubatch above 4096 now builds, and is four times slower

Status: **REFUTED with numbers.** Baseline `37ac0f1d` unchanged (prefill 3122.4,
decode 78.225).

## What blocked it, found statically

Three instrumentation probes failed to name the check, so it was located by
reading the composed unit: two graph-node guards validate the **token count** of
a node, and both stopped at 4096.

```
q35_gdn_layer:  ... || count <= 0 || count > 4096 || !qg_shape(x, hidden, count, 1) ...
q35_moe_ffn:    ... || count <= 0 || count > 4096 || !qm_shape(x, hidden, count, 1) ...
```

`count` is the number of tokens in the node, so a 6144- or 8192-token step failed
graph construction before the first expert projection — which is exactly what the
probes had observed (step entry guard passing, no `mt:` projection line, HTTP 500
without text). Both guards are raised to 8192 in the composed unit and in
`goose/model/qwen35-gdn.goose` and `goose/model/qwen35-trunk.goose`.

## The graph now builds

Candidate `ae7310ac…` (the raised guards plus the baseline `cuda-driver.o`),
`--batch-size 8192 --ubatch-size 8192`, same profile otherwise:

| request | result | fills | upload |
| --- | --- | ---: | ---: |
| 4096 tokens, 1 predicted | HTTP 200, `prompt_ms` 1616.8, 2533.4 tok/s | 8 107 | 15.4 GB |
| 32768 tokens, 1 predicted | HTTP 200, `prompt_ms` **51 120.4**, **641.0 tok/s** | 53 357 | 101.5 GB |

The 4096-token request behaves like the baseline (1600 ms class), so the guard
change itself is neutral.

## Why the lever fails

The baseline needs 12 208-12 312 ms for 32768 tokens over eight 4096-token
passes, that is about 1.5 s per pass. At ubatch 8192 there are four passes of
8192 tokens, and each one costs about 12.8 s: the per-pass attention and mask
work grows with the ubatch far faster than the pass count falls, because the
mask is `n_kv x n_tokens` and the tiled attention covers the whole ubatch. Total
51 s against 12.2 s — **four times slower**, and six times short of the 3500
target instead of 8 % short.

The pass-count lever is therefore refuted: halving the passes does not halve the
expert traffic in wall time, it multiplies the attention cost.

## Artefacts

`runs/ub8192-023312` (4096-token success), `runs/ub8k-32k-023344` (32768 tokens,
51 s), `runs/ubprobe3-*` (the three failing probes), build tree
`/tmp/opencode/apex-ub`.
