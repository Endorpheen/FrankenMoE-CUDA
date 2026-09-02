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

## EXP-2026-09-01-002-io-lane-trace

- Status: `REJECTED` (hypothesis).
- Hypothesis: the 44 ms/token drain persists because heterogeneous random reads leave work unevenly distributed across four lanes, so the slowest lane gates layer completion.
- Bottleneck: unchanged drain wait despite moving page commits earlier.
- Change: no runtime behavior change; enable `--io-trace` for one 64-token baseline run.
- Primary metric: per-lane total requested bytes and total/percentile read latency.
- Supporting metrics: request-size distribution, file-offset adjacency, per-projection balance, and effective bandwidth.
- Risks: trace locking can perturb absolute speed, so this run is diagnostic and will not replace the performance baseline.
- Acceptance: the trace must account for all demand reads and identify a measurable imbalance or adjacency opportunity that defines one isolated follow-up experiment.

### Results

- Trace: 23,247 demand reads and 14,734.5 MiB of aligned physical traffic.
- Request sizes: 15,050 x 524,800 bytes; 448 x 704,000 bytes; 7,749 x 921,600 bytes.
- Read latency: 1.919 ms median, 2.859 ms p95, 3.288 ms p99, and 5.913 ms maximum.
- Lane balance: 5,779-5,839 reads and 3,657-3,718 MiB per lane; no material imbalance.
- Decode reads were slower than prefill reads: 2.122 ms versus 0.900 ms median.
- Exact adjacency: 9.02% of within-layer/projection pairs; merging them can remove about 6.6% of calls without over-read.
- A 1 MiB gap threshold can merge 16.56% of pairs but adds 5.54% traffic; larger gaps are unattractive.
- Decision: `REJECTED` hypothesis. The slowest lane is not systematically gating progress; work is balanced. The trace supports testing eight lanes before implementing limited exact-adjacency coalescing.

## Next experiment

`EXP-2026-09-01-003-eight-io-lanes`: change only `--io-threads 4` to `8` and compare five full runs. Accept only if median generation improves by at least 3% without a warm-window, CPU, memory, or stability regression.
