# EXP-2026-11-23-1817 — Prefill ubatch above 4096 is a functional limit, not just a guard

Status: **INVESTIGATION, no gain.** Baseline `37ac0f1d` (prefill 3122.4,
decode 78.225) unchanged.

## Why this was tried

The expert traffic scales with the number of prefill passes. The accepted run
reports `960 projections = 8 ubatches x 40 layers x 3 planes` and 55.3 GB
uploaded, that is 6.92 GB of misses per ubatch. Four passes instead of eight
would halve the traffic, and PCIe is the measured floor of the request
(51.5 GB of expert DMA at 8.5 GB/s is 6.0 s of the 12.5 s prompt).

## What was raised

Three limits, all in the Goose model:

- `goose/model/qwen35-generation.goose`: `Q35_MAX_PREFILL_BATCH` 4096 -> 8192
  and the staging buffer `var batch: i32[4096]` -> `i32[8192]`;
- `goose/model/qwen35-step.goose`: the step guard `tokens.len > 4096` -> 8192.

The composed unit was rebuilt with the repository's own toolchain
(`goose --ast-budget-mib 2048`, 2m32s, then `gcc -O1` on the 46 MB `main.c`,
3m58s) and linked with the baseline `cuda-driver.o` and the remaining archive
members. Candidate SHA-256 `44d329b6…`, then `22df135d…` after the buffer fix.

## Result

| batch/ubatch | prompt | outcome |
| ---: | ---: | --- |
| 4096 | 4096 | HTTP 200, `prompt_ms` 1603.8, 2554 tok/s — the baseline path still works |
| 6144 | 6144 | HTTP 500 `generation failed` |
| 8192 | 8192 | HTTP 500 `generation failed` |
| 8192 | 32768 | HTTP 500 `generation failed` |

Before the buffer fix the server aborted with
`goose runtime error: array index out of bounds`; after it the server survives
and the generation step returns an error instead, with no message in the log.

## What this means

4096 is a functional limit of the current runtime: raising the three Goose-level
limits is necessary but not sufficient, and at least one more limit sits in the
step or CUDA path (the error is silent, so it needs instrumentation inside
`launch_matrix`/`launch_mmq` to name the failing check). Every iteration costs a
full Goose rebuild (about 6.5 minutes), and the payoff is uncertain: a larger
ubatch changes the attention tiling and therefore the exact output, which the
acceptance criteria forbid.

Recorded so the next attempt starts from the three limits already raised, the
candidate hashes, and the exact failure mode.

## Artefacts

`runs/ub8k-001401` (abort), `runs/ub8k2-002456` and `runs/ub8k3-002636` (500s),
`runs/ub6k-002717` (4096 works, 6144 fails); build tree `/tmp/opencode/apex-ub`
with `main.goose` carrying the raised limits.
