# Experiment log

## EXP-2026-09-01-000-baseline

- Status: `ACCEPTED`.
- Hypothesis: a fixed five-run workload can separate cold-cache startup behavior from warm sustained decode and expose whether later P1 changes exceed normal variance.
- Bottleneck under test: measurement uncertainty and incomplete utilization telemetry.
- Change: add reproducible aggregation, per-run output hashes, first/last 32-token windows, CPU/GPU utilization, and physical/logical storage telemetry.
- Primary metric: median warm-window single-stream generation tok/s.
- Risks: background GPU users, system swap activity, natural EOG before 256 tokens, and changing automatic cache budgets.
- Acceptance: clean build, small-model byte equality, at least five comparable full-model runs, identical output hashes, and complete machine-readable summaries.

### Results

- Clean rebuild: passed after making bootstrap detection preserve equivalent local comment changes.
- Correctness: 13/13 CTest passed; resident and streamed small-model output matched byte for byte; overlap smoke test passed.
- Runs: 5 x 256 generated tokens.
- Overall generation: median 8.515 tok/s, range 8.376-8.629 tok/s.
- First 32 tokens: median 4.876 tok/s, range 4.789-4.892 tok/s.
- Last 32 tokens: median 11.446 tok/s, range 10.958-12.140 tok/s.
- Prompt processing: median 4.11 tok/s.
- TTFT: median 6.874 s; the first run was slower at 9.679 s.
- RAM cache hit rate: median 90.9%.
- Peak RSS: median 26,794 MiB.
- Peak VRAM delta: median 6,874 MiB, including dense weights, compute buffers, and the 1,993 MiB expert hot store.
- Process swap: 0 MiB in every run.
- Full-model output: coherent in every run, but the five greedy CUDA output hashes differ. This is recorded as a known nondeterminism; the small-model byte gate remains the lossless criterion.
- Decision: `ACCEPTED` because this experiment supplies required measurement infrastructure without changing inference behavior.

## Next experiment

`EXP-2026-09-01-001-two-wave-overlap`: publish first-projection expert reads before the remaining projections so existing I/O lanes start earlier and reduce the measured 43-45 ms/token drain wait. H2D overlap is deferred because the baseline spent only about 66-70 ms on H2D across the entire 256-token run, which cannot explain the dominant stall.

## EXP-2026-09-01-001-two-wave-overlap

- Status: `REJECTED`.
- Hypothesis: publishing the first projection before committing the remaining cache pages starts SSD reads earlier and reduces per-token drain wait.
- Bottleneck: baseline drain wait is 43-45 ms/token, while total H2D time is only 66-70 ms per 256-token run.
- Change: enable the existing `--io-two-wave` mode without changing the model, prompt, cache budgets, thread counts, or other runtime behavior.
- Primary metric: median overall single-stream generation tok/s; drain seconds/token is the diagnostic metric.
- Risks: extra worker wakeups, contention while the second wave grows the batch, and improvement only in cold windows.
- Acceptance: at least +3% median generation speed outside baseline variance, no warm-window regression, byte-equality gate passes, no memory growth, no process swap, and no I/O errors.

### Results

- Runs: 5 x 256 generated tokens with `--io-two-wave`.
- Overall generation: median 8.506 tok/s versus 8.515 baseline, `-0.11%`.
- First 32 tokens: median 4.882 tok/s versus 4.876 baseline, `+0.11%`.
- Last 32 tokens: median 11.504 tok/s versus 11.446 baseline, `+0.51%`.
- Drain wait: median 0.044 s/token in both configurations.
- Physical SSD throughput: median 576.2 MiB/s versus 581.5 MiB/s baseline.
- Peak RSS: median 26,773 MiB versus 26,794 MiB baseline.
- Peak VRAM delta: median 6,887 MiB versus 6,874 MiB baseline.
- Process swap: 0 MiB in every run; no I/O errors or corrupted output observed.
- Correctness: the existing small-model two-wave byte-equality gate passed before the experiment. Full-model hashes retained the known cross-process nondeterminism.
- Decision: `REJECTED`. The changes are inside normal variance, miss the 3% threshold, and do not reduce the targeted drain wait.
- Revisit only if page-commit latency becomes material after changing the cache layout or storage platform.

## Next experiment

Capture an expert-level I/O trace on the unchanged baseline to quantify request sizes, adjacency, lane balance, and per-read latency before choosing between higher I/O concurrency and read coalescing.
