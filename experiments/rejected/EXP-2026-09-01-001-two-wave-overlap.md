# EXP-2026-09-01-001-two-wave-overlap

## Decision

`REJECTED`: two-wave publication did not improve median generation throughput or reduce the targeted drain wait.

## Hypothesis

Committing only the first present expert projection, publishing those reads immediately, and committing the remaining projections while I/O workers run should move SSD work earlier and reduce the 43-45 ms/token drain wait.

## Configuration

- Source baseline: commit `0b45b4c` with inference code still based on `ed95e46`.
- Model: Qwen3.8-Flash-Next `UD-IQ3_XXS`, verified three-shard set.
- Prompt: `Briefly explain why addressed expert loading is useful for MoE models.`
- Five fresh-process runs, 256 tokens, context 4096, ubatch 256.
- 16 compute threads, 4 I/O lanes, O_DIRECT, SSD/CPU overlap.
- 32 GiB RAM cache ceiling and 2 GiB VRAM expert-cache request.
- Only changed runtime option: `--io-two-wave`.

## Results

| Run | Generation tok/s | First 32 tok/s | Last 32 tok/s | Drain s/token | RSS peak MiB | VRAM delta MiB | Swap MiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 8.472 | 4.883 | 11.504 | 0.045 | 26,773 | 6,889 | 0 |
| 2 | 8.588 | 4.882 | 11.649 | 0.043 | 26,450 | 6,888 | 0 |
| 3 | 8.570 | 4.877 | 11.825 | 0.044 | 26,716 | 6,854 | 0 |
| 4 | 8.506 | 4.860 | 10.612 | 0.044 | 27,069 | 6,887 | 0 |
| 5 | 8.475 | 4.893 | 11.025 | 0.045 | 27,255 | 6,860 | 0 |
| Median | 8.506 | 4.882 | 11.504 | 0.044 | 26,773 | 6,887 | 0 |

Baseline medians were 8.515 tok/s overall, 4.876 tok/s for the first 32 tokens, and 11.446 tok/s for the last 32 tokens. Overall change was `-0.11%`; warm-window change was `+0.51%`. Both are inside ordinary variance and far below the 3% acceptance threshold. Median physical read rate declined from 581.5 to 576.2 MiB/s.

## Correctness and stability

The existing `G4d` small-model gate verifies byte equality for two-wave publication and passed in the P0 test run. Every full-model run completed with coherent output, zero process swap, no row-stream errors, and stable memory use. Full-model output hashes differed as they did in the baseline.

## Why it failed

The page commits moved off the path to the first projection, but they were not a material part of the measured drain. The workers still had to read the same bytes, and the second-wave queue growth plus extra wakeups did not change the point at which compute caught the I/O lanes.

## Revisit conditions

Revisit on a platform with expensive virtual-memory commit operations, after changing cache layout, or if an I/O trace shows workers idle while the eval thread commits remaining projections.

The exact implementation patch is saved beside this report.
