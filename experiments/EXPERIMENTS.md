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

## EXP-2026-09-01-003-eight-io-lanes

- Status: `REJECTED` as a general speedup; retained as a documented cold-start profile for `bmoe-cli` (resolved by the EXP-004 deterministic retrial below).
- Hypothesis: eight independent O_DIRECT lanes reduce per-layer completion latency enough to lower drain wait, despite extra CPU and storage queue contention.
- Bottleneck: 23,247 small reads with 2.122 ms median decode latency and 44 ms/token median drain wait.
- Change: `--io-threads 8` instead of `4`; all other baseline parameters remain fixed.
- Primary metric: median overall single-stream generation tok/s; drain seconds/token is diagnostic.
- Risks: CPU contention with 16 compute threads, higher p99 storage latency, no benefit if the per-layer dependency chain rather than lane count is limiting.
- Acceptance: at least +3% median generation speed, no warm-window regression, stable RAM/VRAM, zero process swap, no I/O errors, and correctness gate unchanged.

### Results

- Five 256-token runs improved median overall generation from 8.515 to 8.958 tok/s (`+5.20%`) and reduced median drain wait from 0.044 to about 0.037 s/token.
- The same runs improved the first 32-token window by `+11.48%`, but the last 32-token window moved by `-1.40%`, inside substantial route-dependent variance.
- Five paired 512-token runs improved the first 32-token window by `+20.74%` and the last 32-token window by `+1.54%`, while overall generation moved by `-1.38%`.
- The paired configurations followed different generated-token and expert-routing trajectories. The eight-lane runs consequently read about 1.6 GiB more expert data and used 5.28% more peak RSS, so neither delta isolates lane count.
- An additional five-pair repetition was stopped to conserve the available execution quota; incomplete runs are excluded.
- Decision: `INCONCLUSIVE`. Eight lanes clearly reduce early drain wait, but the existing benchmark cannot determine their sustained benefit because each process executes a different expert workload.

### Deterministic retrial (EXP-2026-09-01-004 infrastructure)

- Three interleaved 4-lane/8-lane pairs over one fixed 256-token workload: overall 8.274 -> 8.753 tok/s median (`+5.80%`; pairs `+5.92/+6.08/+5.52%`), first-32 window `+12.4%` with non-overlapping distributions, last-32 window `-0.13%` (no change), median drain 0.043 -> 0.037 s/token.
- Resolution: eight lanes accelerate only the cache-fill phase; warm throughput, RAM (peaks overlap at 25.9-26.4 GiB), and swap are unchanged. The +3% sustained-generation acceptance bar is therefore not met.
- `--io-threads 8` remains available as a cold-start profile for short interactive `bmoe-cli` runs. The interactive server (`scripts/run_qwen38_server.sh`) runs the separate expert-tier llama.cpp binary and is unaffected either way.

## EXP-2026-09-01-004-deterministic-replay

- Status: `ACCEPTED` (measurement infrastructure).
- Hypothesis: forcing both configurations through identical prompt tokens, output tokens, and per-layer expert IDs removes route-dependent work variance and makes a smaller paired benchmark conclusive.
- Bottleneck under test: benchmark nondeterminism, not an inference optimization.
- Change: capture one natural workload in a compact text file, then replay its token and expert route sequence without enabling the expensive route-trace barriers.
- Primary metric: exact equality of replayed token IDs and expert-route records; performance is secondary until this correctness gate passes.
- Risks: graph shapes can differ between capture and replay, route modifiers could conflict with replay, and instrumentation in the eval callback could perturb timing.
- Acceptance: fail closed on every metadata/shape mismatch, reproduce the captured output and route count exactly, pass the existing test suite and clean build, and add negligible steady-state work beyond copying the already materialized top-k IDs.

### Results

- Capture/replay runs through the existing top-k eval barrier, deliberately after every route modifier, and never dereferences tensor storage directly: the hook gathers and writes the IDs backend-aware, which is what fixed a first-run segfault (the top-k view can live in a GPU buffer).
- Tiny model (16 tokens): capture plus two replays produced byte-identical token streams; 76 route records consumed exactly. Wrong prompt, changed top-k, a deleted route record, and an edited route shape each failed closed with a specific error surfaced through the graph abort path.
- Full model, 64-token proof (4-lane capture, then 4-lane and 8-lane replays): all three token streams byte-identical; logical expert demand exactly 905.8 MiB in every run; physical expert reads 8471.4 / 8471.4 / 8472.9 MiB — the four-lane replay matched the capture to 0.1 MiB.
- Full model, 256-token fixed workload, three interleaved 4-lane/8-lane pairs plus the capture: all seven outputs byte-identical, 12,336 route records consumed exactly, demand 905.8 MiB everywhere, cache hit 90.8% everywhere, process swap zero, peak RSS 26-27.6 GiB in both arms.
- Fixed-workload lane effect (this retrial of EXP-003's question, accepted as evidence for the infrastructure only): overall 8.274 -> 8.753 tok/s median (`+5.80%`; pairs `+5.92/+6.08/+5.52%`), first-32 window `+12.4%` with non-overlapping distributions, last-32 window `-0.13%` (no change), median drain 0.043 -> 0.037 s/token overall and 0.1246 -> 0.1024 in the first 32. Eight lanes accelerate the cache-fill phase; sustained warm throughput is unchanged.
- Replay overhead: matched-trajectory comparisons (capture vs replay over the same 64-token workload) measured about `1.3%`; a natural four-lane 256-token run without workload flags read 5% fewer expert bytes by route luck, so single-run natural comparisons overstate it. Both arms of an A/B replay the same workload and pay the same overhead.
- The workload file records arch, layer count, and effective top-k but not a model hash; `benchmarks/baseline.json` already pins the shard SHA-256 values, which is the identity gate for local A/Bs.
- Decision: `ACCEPTED`. The gate reproduced outputs, tokens, and routes exactly; every corruption attempt failed closed; 13/13 tests plus the CUDA equality gates passed after the change.

## EXP-2026-09-01-005-expert-tier-baseline

- Status: `ACCEPTED` (the practical speed baseline for Qwen3.8 shifts from bmoe-cli to the public expert-tier server).
- Subject: the clean public expert-tier `llama-server` (state A: `upstream/llama.cpp-expert-tier` at `4aaad5d`, built in `build/expert-tier-cuda`; provenance verified via CMakeCache `CMAKE_HOME_DIRECTORY` and `ldd`).
- Parameters: Igor's interactive profile — `-c 64000 -ctk q4_0 -ctv q4_0 --reasoning-effort low -ngl 99 --cpu-moe -ehs -1 -fa on --jinja -t 16`; greedy requests (`temperature 0`) through `/v1/chat/completions` streaming; model shards and SHA-256 as pinned in `benchmarks/baseline.json`.
- Protocol: one server process with a cold request, a warmup, five repeated main-prompt runs (256 tokens), then three distinct-prompt scenarios (coding 256, free text 256, long 512); a second process for restart behaviour. Measured by wall clock around each streamed request; server print_timing cross-checked the cold prefill.

### Results

- Repeated-prompt warm generation: median `18.09 tok/s` (18.00-18.16 across five runs) — the hot store fully adapted to one repeated prompt.
- Distinct-prompt scenarios: coding `13.5`, free text `14.8`, long 512-token `15.4 tok/s` — the earlier "14-15 tok/s interactive" observation is the distinct-prompt band, not the repeated-prompt peak.
- Cold start, clean re-measure (model pages dropped with `posix_fadvise`, no other load; read from the server's own print_timing log): startup 18.9 s, prefill 50 tokens in 6.5 s (7.7 tok/s), generation `~12.1 tok/s` (25.6 s for 310 tokens total).
- Cold with a warm OS page cache (process restart): 15.8-16.3 tok/s, startup 5-8 s. Warm TTFT 0.2-0.4 s; cold TTFT 1.6-2.0 s.
- Peak RSS 42.7 GiB (the upstream loader faults a whole 30 GiB shard at load).
- Correction (2026-09-01, later the same day): the originally recorded cold figures — `1.025 tok/s`, 13.7 s prefill, and `435 MiB` of silent process swap — came from a run that overlapped a parallel 16-thread build on the same CPU (`--cpu-moe` generation is CPU-bound), so they are retracted. The clean numbers above replace them. The 435 MiB swap observation is not confirmed by the clean run and must be re-established before being cited as a fact.
- Decision: `ACCEPTED` as the new practical baseline. The honest interactive number to beat is the distinct-prompt band (13.5-15.4 tok/s); cold start is a ~12 tok/s phase from an empty model cache, not a cliff.

## EXP-2026-09-01-006-expert-tier-franken-patches

- Status: `ACCEPTED` (stability and memory improvement, speed-neutral).
- Subject: our `patches/expert-tier-integration.patch` on top of the same public fork (state B: `work/llama.cpp-integration`, built separately in `build/expert-tier-franken-cuda`).
- Change under test: the loader fix (`init_mappings(use_mlock, ...)` — no unconditional MAP_POPULATE), pinned host staging for hot-store uploads, and the telemetry counters; nothing else differs from state A.
- Protocol: identical to EXP-005, run back-to-back on the same machine state; outputs compared byte-wise per prompt.

### Results

- Warm repeated-prompt generation: A `18.09` vs B `17.99 tok/s` median (`-0.55%`, inside the 17.3-18.6 spread) — no speed regression.
- Distinct-prompt scenarios (single runs each): coding 18.96 -> 17.11 s, free 17.25 -> 16.44 s, long 33.34 -> 30.46 s — all three faster on B, but one run each, recorded as "no regression" rather than a claimed win.
- Peak RSS: A `42.7 GiB` vs B `29.8 GiB` (`-12.9 GiB`), visible from the first minute and constant afterwards — the loader fix removes the whole-shard fault storm while leaving demand paging in place.
- Pinned staging is active (the patch logs a warning when staging is unavailable; none appeared in B's log).
- Output correctness: byte-identical text between A and B on all four prompts (main, coding, free, long).
- Swap: 0 in both arms on a warm cache. The originally reported 435 MiB cold swap on A is retracted (contaminated run, see the EXP-005 correction); cold-swap behaviour of either arm remains unmeasured.
- Decision: `ACCEPTED` as a stability improvement: same speed, same bytes, 12.9 GiB less RAM. Speedups on top of the expert-tier baseline remain unclaimed and are Phase 7+ work.
