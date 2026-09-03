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

## EXP-2026-09-01-007-swap-watchdog-in-server

- Status: `ACCEPTED` (safety mechanism, speed-neutral).
- Subject: Phase 4 mechanism 1 — the external swap watchdog of `scripts/run_qwen38_server.sh` moved into the server itself, so the protection no longer depends on the launcher.
- Change under test: `--swap-watchdog` flag (`common/common.h`, `common/arg.cpp`, env `LLAMA_ARG_SWAP_WATCHDOG`) plus a monitor thread in `tools/server/server-swapwatchdog.{h,cpp}`. The thread samples `/proc/self/status` (VmRSS, VmSwap) and `/proc/vmstat` (pswpin/pswpout) once per second and stops the server (SIGINT, escalation to SIGKILL after 30 s) when:
  - process VmSwap grows by over 256 MiB within one sample;
  - system swap I/O exceeds 2048 MiB/s in one sample; or
  - system swap I/O stays above 512 MiB/s for 5 consecutive samples.
  Thresholds mirror the launcher script. Env overrides of the thresholds exist as a test hook for small cgroup tests and are never set by the launcher.
- Functional gate (tiny synthetic MoE model, 7 MB):
  - burst path: a neighbouring cgroup hog under `MemoryHigh=1M` produced ~4 MiB/s of system swap-out; with the hard threshold lowered the watchdog tripped 4 s after start, logged the reason and the unit exited cleanly (`inactive`, no kill signal needed).
  - no-false-trip path: normal runs with production thresholds never tripped; generation completed.
  - one bug was found and fixed during the gate: the `/proc/self/status` parser compared `VmRSS: ` with a trailing space while the kernel writes a tab, so every sample failed and the loop never reached the swap checks.
- Speed gate (Qwen3.8-Flash-Next UD-IQ3_XXS, 64K ctx, `--cpu-moe`, warm cache, patched build with `--swap-watchdog`): measured warm greedy run `16.26 tok/s` (decode, 256 tokens, prefill 0.24 s) after a 13.82 tok/s warmup run — inside the accepted warm band (13.5-15.4 distinct, up to 18.09 repeated), no regression. VmSwap 0 the whole run, no watchdog events.
- Cold-start note: the watchdog is active during model loading; the clean cold run of B (EXP-005 correction, EXP-006) showed zero process swap, and the file-backed model read does not count as swap.
- Limitation: under artificial cgroup throttling (`MemoryHigh` on the server's own cgroup) the monitor thread itself is stalled by the kernel like every other thread in the cgroup; real burst scenarios do not self-throttle the server, so this does not affect production behaviour.
- Decision: `ACCEPTED`. The launcher keeps its external monitor for CSV telemetry; the in-server watchdog is the safety layer that works without it.

## EXP-2026-09-01-008-ram-headroom-check

- Status: `ACCEPTED` (safety mechanism, startup gate).
- Subject: Phase 4 mechanism 2 — the launcher's RAM headroom check moved into the server itself, so the gate works no matter how the server is started.
- Change under test: `--ram-headroom-gib N` (`common/common.h`, `common/arg.cpp`, env `LLAMA_ARG_RAM_HEADROOM_GIB`, default 8, 0 = off) plus a one-shot `MemAvailable` check in `tools/server/server.cpp` before backend init and model load. Below the threshold the server fails closed with a clear error and exit code 1.
- Functional gate (tiny synthetic MoE model, 7 MB, CPU-only, no GPU):
  - refusal path: `--ram-headroom-gib 200` with 48.0 GiB available -> the server refused to start, logged `insufficient RAM headroom: 49180 MiB available, 200 GiB required`, exit code 1.
  - pass path: default 8 GiB with 48.0 GiB available -> the server started and logged `RAM headroom: 49085 MiB available, 8 GiB required`.
- The gate matches the one `scripts/run_qwen38_server.sh` performs before exec (8 GiB minimum); the launcher check stays as a first line, the in-server check is the layer that works without the launcher.
- Decision: `ACCEPTED`. No speed measurement required: the check runs once before any allocation and adds no runtime path.

## EXP-2026-09-01-009-vram-reserve-ehs-autofit

- Status: `ACCEPTED` (safety mechanism; reveals a silent upstream bug).
- Subject: Phase 4 mechanism 3 - VRAM reserve and bounded EHS autofit.
- Silent bug found: with the production launch flags (`-ngl 99` plus `-ehs -1`) the upstream autofit never ran. `common_fit_params` aborts because `n_gpu_layers` is user-set (`fit: n_gpu_layers already set by user to 99, abort` in every server log), the `-1` placeholder fell through, and `llama_expert_hotstore::allocate` returned false on `hot_s <= 0`. The expert hot store was silently DISABLED in every Qwen server run so far, including the accepted EXP-005/006 baselines (no expert lines in any server log).
- Change under test:
  - `llama_expert_hotstore::allocate` now implements `-1` (autofit) directly: slot cost is computed from the per-expert slice sizes of the entries, and the slot count is `free VRAM - reserve` divided by the slot cost, clamped to `n_experts`. The decision is logged.
  - new flag `--ehs-reserve-mb N` (`common/common.h`, `common/arg.cpp`, env `LLAMA_ARG_EHS_RESERVE_MB`, default 0) keeps N MiB free on the GPU for processes that start after the server; wired through `llama_context_params`.
  - the hot-store allocation failure message now reports the reserve.
  - `scripts/run_qwen38_server.sh` finally passes its `VRAM_RESERVE_MB` (default 2048) to the server; the variable existed since the launcher was written but was never forwarded.
  - `patches/expert-tier-integration.patch` was rebuilt as original integration plus the safety mechanisms; it applies cleanly to the pinned upstream commit and reproduces the benchmarked binary functionally (the preserved local comment edits in the worktree stay out of it).
- The first implementation estimated only live expert bytes and was unsafe: 15 estimated slots required 2264 MiB after the eight sentinel lanes and allocator alignment were included. Exact layout sizing then exposed CUDA fragmentation, and an allocation that consumed all reported free VRAM failed during the first graph warm-up.
- Final safety behavior: binary-search the exact CUDA layout, retry with fewer slots after allocation failure, preserve `-ehs -1` when the upstream fit aborts on explicit `-ngl`, initialize routing state after the final slot count is known, and keep a mandatory 1024 MiB runtime margin in addition to the caller reserve.
- Functional gate: with `--ehs-reserve-mb 0`, one live slot allocated as a 996 MiB buffer, the server reached the listening state, generated 16 coherent reasoning tokens, used zero process swap, and shut down cleanly. With the launcher's 2048 MiB reserve, autofit safely selected zero slots.
- Decision: `ACCEPTED` as allocation-safety infrastructure only. Performance activation is governed by EXP-010 and remains opt-in.

## EXP-2026-09-01-010-ehs-ab

- Status: `REJECTED` (hot-store activation); the launcher defaults to `EHS=0`.
- Hypothesis: a live GPU expert slot improves repeated and sustained decode by avoiding CPU expert work without changing greedy output.
- Protocol: patched server, Qwen3.8 UD-IQ3_XXS, 64K context, greedy output, identical prompt and parameters, five 256-token warm runs per arm plus cold, warmup, and 512-token long requests. Both arms used `-ngl 47` because `-ngl 99` left only 1943 MiB free and safe autofit correctly selected zero slots; 47 GPU layers made one 996 MiB live slot fit. Raw per-run results are in `benchmarks/exp010-ehs-ab.json`.
- Warm decode median: hot store off `15.419 tok/s` (range `14.097-15.746`) versus one-slot autofit `14.199 tok/s` (range `13.983-14.272`), a `-7.91%` regression.
- Long decode: `14.992` versus `13.312 tok/s`, a `-11.21%` regression. Cold decode: `15.148` versus `13.593 tok/s`, a `-10.27%` regression.
- Correctness: all five off-arm greedy outputs had the same SHA-256. The live-hot-store arm produced five different hashes across the first five repeated requests and only stabilized for the last two, so the correctness gate failed.
- Memory: process swap remained zero in both arms. End RSS was 36.18 GiB off versus 38.50 GiB with autofit; total GPU use was about 9.79 versus 10.91 GiB. Physical-read totals are not comparable because the arms ran sequentially against a warming OS page cache.
- Decision: `REJECTED`. The hot store is disabled by default and is retained only behind explicit `EHS=-1` for future debugging. It must not become the baseline until both output stability and the speed regression are fixed.

## EXP-2026-09-02-011-expert-tier-bottleneck-profile

- Status: `ACCEPTED` (measurement infrastructure; no inference-path change).
- Hypothesis: with `EHS=0`, Qwen3.8 decode is primarily the stock CPU `MUL_MAT_ID` path over mmap-backed expert tensors; the useful P1 target is either page-cache latency or CPU vector-dot work, not H2D transfer.
- Bottleneck under test: time and resource split between mmap page faults/physical reads, CPU execution, and GPU utilization during cold and warm decode.
- Plan: run the unchanged patched server at the accepted 64K profile and record one cold 256-token request followed by one warm identical request. Collect server timings, output hashes, `/proc/<pid>/{stat,status,io}` deltas, and half-second GPU utilization samples. No cache dropping and no performance claim from this diagnostic run.
- Primary metrics: generation tok/s, process physical-read bytes, major/minor faults, CPU task time/utilization, GPU utilization, RSS, VRAM, and process swap for cold versus warm requests.
- Risks: the OS page cache may already be warm, system-wide GPU activity can contaminate utilization, and attaching profilers can perturb timing. The trace therefore selects the next hypothesis but does not become a speed baseline.
- Acceptance: measurements must distinguish CPU compute from storage wait well enough to choose exactly one next experiment. Output hashes must remain unchanged and process swap must stay zero.

### Results

- Cold request: `14.418 tok/s`, 2905.94 MiB of physical reads, 62,192 major faults, 797,703 minor faults, and 29,592 MiB peak RSS. The request used 293.37 CPU-seconds over 20.19 seconds of server time, equivalent to about 14.5 fully occupied CPU cores. Mean GPU utilization was 24.0%.
- Warm identical request: `17.679 tok/s`, 0.27 MiB of physical reads, 6 major faults, 116,144 minor faults, and 29,969 MiB peak RSS. It used 232.96 CPU-seconds over 14.63 seconds, equivalent to about 15.9 occupied CPU cores. Mean GPU utilization was 31.2%.
- Correctness: cold and warm SHA-256 hashes were identical. Process swap stayed at zero. Raw counters and half-second samples are stored in `benchmarks/exp011-profile.json`.
- Interpretation: storage faults explain the first-request penalty, but they disappear after page-cache warm-up while all CPU cores remain saturated and GPU utilization stays low. RAM prefetch can improve cold or changing-expert workloads; it cannot raise the warm sustained ceiling by itself.
- Decision: `ACCEPTED`. The next isolated hypothesis is asynchronous `POSIX_MADV_WILLNEED` for all experts selected by one CPU `MUL_MAT_ID`, issued before computing the first selected expert. It targets cold latency only and must remain opt-in until a five-run A/B proves a gain without increasing RSS or changing output.

## EXP-2026-09-02-012-selected-expert-madvise

- Status: `REJECTED`.
- Hypothesis: after `MUL_MAT_ID` groups rows by selected expert, issuing `POSIX_MADV_WILLNEED` for every selected mmap-backed expert slice before the worker barrier lets Linux overlap later expert page-ins with computation of the first expert.
- Bottleneck under test: 2.91 GiB of physical reads and 62,192 major faults observed during the first 256-token request in EXP-011.
- Change: an opt-in `LLAMA_CPU_MOE_PREFETCH=1` path in the stock CPU `MUL_MAT_ID` kernel. Thread zero advises each selected expert slice once before releasing the worker barrier. The disabled path keeps the existing computation and ordering.
- Primary metric: cold generation tok/s. Secondary metrics: physical-read volume, major faults, warm generation tok/s, RSS, CPU time, GPU utilization, and process swap.
- Risks: syscall overhead, excessive readahead, page-cache pollution, increased RSS, or no overlap because advice is issued too late. The optimization must not change tensor contents, routing, or output hashes.
- Acceptance: five comparable runs per arm; at least 3% median cold-generation improvement outside normal spread, identical output hashes, zero swap, no material warm regression, and no unexplained memory growth.

### Results

- Protocol: five interleaved off/prefetch pairs. Every arm started a fresh server after dropping all three model shards from the OS page cache, then ran one cold and one immediately repeated warm 256-token request.
- Cold generation: `11.744 tok/s` median without prefetch (range `11.586-11.809`) versus `10.827 tok/s` with prefetch (range `10.764-10.917`), a `-7.81%` regression.
- Warm generation: `17.596 tok/s` median without prefetch (range `17.407-17.928`) versus `15.473 tok/s` with prefetch (range `15.358-15.707`), a `-12.07%` regression.
- The advice reduced median cold physical reads from `26,052.0` to `24,888.9 MiB` (`-4.46%`) and major faults from `266,237` to `182,945` (`-31.28%`), but the per-operation advice overhead and competing readahead cost more time than they saved.
- Median cold RSS decreased from `29,475.6` to `29,031.2 MiB` (`-1.51%`). Process swap remained zero in every request.
- Correctness passed: every off and prefetch request produced the same SHA-256 (`3be17501f0987ac5881160853f361a274ba40379a14f283b9fa13f61680c11e2`).
- Decision: `REJECTED`. The runtime code was removed. Revisit prefetch only when selected reads can be batched and issued earlier than `MUL_MAT_ID`, rather than making repeated advice calls in the compute path.

## EXP-2026-09-02-013-cpu-thread-scaling

- Status: `ACCEPTED` (12-thread balanced profile); 24 and 32 threads are rejected.
- Hypothesis: reducing CPU contention may retain the warm `EHS=0` throughput of the 16-thread profile while releasing cores for the desktop and other services.
- Bottleneck under test: EXP-011 measured about 15.9 occupied CPU cores, negligible warm physical I/O, and only 31.2% mean GPU utilization.
- Change: compare `-t 12` against `-t 16`; model, context, prompt, output length, GPU offload, cache quantization, and all other server parameters remain unchanged. Earlier 24- and 32-thread guardrails remain diagnostic only.
- Protocol: three interleaved 12/16 pairs. Each fresh server receives one 256-token warmup request followed by one identical measured 256-token request. The OS model page cache is not dropped because the target is sustained CPU decode, not cold storage behavior.
- Primary metric: median measured warm generation tok/s. Secondary metrics: paired speed deltas, CPU-core utilization, GPU utilization, physical reads, major faults, RSS, process swap, and output hashes.
- Risks: SMT contention may reduce vector throughput, process-to-process routing nondeterminism may change work, or the warmup may not make every selected expert resident.
- Acceptance: at least 3% median and paired speed improvement outside the observed spread, identical output hashes across both arms, zero swap, no material memory growth, and no periodic slow runs.
- Early guardrail: an initial 32-thread warmup was stopped after 106 tokens because generation remained at only `0.69-0.70 tok/s`, versus `17.72 tok/s` for the immediately preceding 16-thread measured request. No 32-thread performance claim is made from the incomplete request; it is excluded from the A/B and establishes only that saturating all logical CPUs is unsafe on the active workstation.

### Results

- Formal three-pair medians: 12 threads `17.615 tok/s` (range `17.387-17.786`) versus 16 threads `17.656 tok/s` (range `17.421-17.765`), a `-0.23%` difference inside noise. Pair deltas were `+2.10%`, `-2.13%`, and `-0.23%`.
- Mean occupied CPU cores fell from `15.93` to `11.95` (`-24.98%`). Median mean GPU utilization was `31.48%` versus `31.93%`; median peak RSS was `29,473.3` versus `29,473.9 MiB`.
- All six measured requests had zero physical reads, zero major faults, zero process swap, and the same 256-token output SHA-256.
- A preceding exploratory pair also agreed: 12 threads `17.754 tok/s` versus 16 threads `17.718 tok/s` (`+0.20%`). It is supporting evidence and is not included in the formal three-pair medians.
- The 24-thread guardrail measured `15.943 tok/s` against `17.718 tok/s` at 16 threads (`-10.02%`). An incomplete 32-thread warmup held `0.69-0.70 tok/s` through 106 tokens and was stopped rather than wasting more than an hour.
- Decision: `ACCEPTED` as a resource-efficiency tradeoff explicitly requested after the three-pair gate. The launcher defaults to `THREADS=12`, retaining effectively identical single-stream speed while leaving four physical cores free. `THREADS=16` remains available; 24 and 32 are rejected.

## EXP-2026-09-02-014-cpu-kernel-profile

- Status: `ACCEPTED` (measurement infrastructure).
- Hypothesis: a small set of CPU expert functions dominates warm decode and hardware counters can distinguish compute throughput from cache/memory stalls before any kernel rewrite.
- Bottleneck under test: at 12 threads the warm path sustains about `17.6 tok/s`, uses about 11.95 CPU cores, performs no physical reads or major faults, and leaves mean GPU utilization near 31.5%.
- Change: no inference-path change. Capture `perf stat` counters and a userspace call-graph sample over one warm 256-token request, then compare its generation speed with an unprofiled request from the same server.
- Primary outputs: hottest symbols and their self/children percentages, cycles, instructions, IPC, cache references/misses, stalled cycles, context switches, and migrations.
- Risks: sampling may perturb throughput, call graphs may be incomplete without frame pointers, and system security policy may block access to performance counters.
- Acceptance: identify a dominant function or measured stall class that selects one isolated optimization; output hash must match, swap must stay zero, and profiler overhead must be reported rather than mistaken for a regression.
- Environment gate: `perf 7.0.12` and userspace symbols are present, but `kernel.perf_event_paranoid=4` blocks all unprivileged events. A temporary change to level 2, restored to 4 immediately after profiling, requires Igor's explicit approval and sudo authentication.

### Results

- Requests: warmup `17.611 tok/s`, unprofiled reference `17.650 tok/s`, and profiled `17.421 tok/s`. Simultaneous `perf stat` plus DWARF sampling overhead was `1.30%`.
- The two dominant compute kernels were `ggml_vec_dot_iq2_s_q8_K` at `29.19%` of sampled cycles and `ggml_vec_dot_iq4_nl_q8_0` at `24.50%`; `ggml_vec_dot_iq3_s_q8_K` added `0.81%`.
- About `39.96%` of sampled cycles landed in a small set of `libgomp` addresses. Local disassembly resolves them to `pause`-based spin loops and their surrounding futex path, so this is OpenMP wait/barrier overhead rather than expert arithmetic.
- Hardware counters: 752.62 billion cycles, 872.42 billion instructions, IPC `1.159`, 10.81 billion cache references, 602.69 million cache misses (`5.58%`), and frontend stalled cycles equal to `0.66%` of cycles. Backend-stall counting is unsupported by this Zen 3 PMU event mapping.
- Correctness and safety: all three outputs had the same SHA-256; physical reads and major faults were zero; process swap was zero; peak RSS stayed near `29,474 MiB`. No samples were lost.
- The 123 MiB raw `perf.data` remains local and is ignored by Git; its SHA-256 is `d8696652ff69d3e1f2015eba1a41722a70ab9d522d14124f74c72d661bdccbd1`. The JSON, counter CSV, and compact symbol report are retained in `benchmarks/`.
- Decision: `ACCEPTED`. The first isolated follow-up is a separate `GGML_OPENMP=OFF` build: compare ggml's persistent native threadpool against the current OpenMP graph execution without changing model parameters or thread count.

## EXP-2026-09-02-015-no-openmp-threadpool

- Status: `REJECTED` (early gate).
- Hypothesis: building with `GGML_OPENMP=OFF` switches graph compute to ggml's persistent internal threadpool and removes a substantial part of the OpenMP spin/wait cycles measured in EXP-014.
- Bottleneck under test: at 12 threads about `39.96%` of sampled warm-decode cycles sit in libgomp `pause`-based spin/wait paths, while the two dominant expert dot-product kernels together account for about `53.7%`.
- Change: build-only. A separate directory `build/expert-tier-franken-no-openmp` is configured identically to the working build (Ninja, Release, CUDA arch 89, shared libs, server, tests, examples, same source `work/llama.cpp-integration`) except `-DGGML_OPENMP=OFF`. No source, model, launcher, or parameter changes; the working build `build/expert-tier-franken-cuda` stays untouched.
- Protocol: small correctness gate first; then, only after Igor's approval, one short exploratory pair OpenMP ON versus OFF at 12 threads, warm 256 tokens each; if OFF does not fail, a reproducible interleaved A/B.
- Primary metric: median warm 256-token single-stream generation tok/s at 12 threads, OFF versus ON. Secondary metrics: paired speed deltas, occupied CPU cores, GPU utilization, physical reads, major faults, RSS, process swap, and output hashes.
- Risks: the internal threadpool also uses a spin barrier; the libgomp cycles may reflect normal waiting for the slowest worker; disabling OpenMP may worsen wake-up latency; the disappearance of libgomp samples does not count as a speedup without a tok/s A/B.
- Acceptance: `ACCEPTED` only at `+3%` median and paired speed outside the observed spread, or a proven CPU-core reduction without noticeable speed loss; identical output hashes across arms, zero swap, no material memory growth, and no periodic slow runs.
- On rejection: the separate build directory may remain as a reproducible artifact, but the working launcher is not switched.

### Results

- Build provenance passed: both builds use the same patched source at upstream commit `4aaad5d`, Release, CUDA architecture 89, shared libraries, and server target. The only material CMake difference is `GGML_OPENMP=ON` versus `OFF`; `ldd` confirms that only the ON CPU library links `libgomp`.
- One immediately paired warm 256-token gate measured OpenMP ON at `17.914 tok/s` (`14,234.77 ms`) and OFF at `17.544 tok/s` (`14,534.60 ms`), a `-2.06%` change. Wall time was `14.63` versus `14.94 s`.
- Mean occupied CPU cores were `11.774` ON and `11.601` OFF (`-1.47%`). Peak RSS was `29,469.9` versus `29,468.0 MiB`; mean GPU utilization was `29.8%` versus `30.1%`.
- Correctness and safety passed: all four warmup/measured outputs produced the historical SHA-256 `3be17501f0987ac5881160853f361a274ba40379a14f283b9fa13f61680c11e2`; measured physical reads, major faults, and process swap were zero.
- The speed delta is within EXP-013's approximately `+/-2%` paired noise, so this is not evidence that OFF is intrinsically slower. It is conclusive only for the acceptance gate: there is no positive signal on speed, CPU use, memory, or GPU use that justifies a long A/B.
- The claim that the measured `39.96%` OpenMP wait cycles "moved" into ggml is not made: the OFF build was not perf-profiled. Source inspection confirms that the native pool has a spin barrier, while this gate proves only that switching pools did not improve user-visible performance.
- Decision: `REJECTED` after the early gate. Keep the working build and launcher on `GGML_OPENMP=ON`. The separate OFF build remains an ignored reproducibility artifact; no baseline update and no source patch are required.

## EXP-2026-09-03-016-mul-mat-id-worker-balance

- Status: `REJECTED` (diagnostic result; no production change).
- Hypothesis: the OpenMP spin/wait samples in EXP-014 are caused by uneven `MUL_MAT_ID` chunk work, so some CPU workers finish their selected-expert work early and wait at the following graph barrier.
- Existing-data gate: the saved EXP-014 `perf.data` assigns `8.03-8.38%` of sampled cycles to each of the twelve graph workers, which excludes a gross single-worker bottleneck. The combined IQ2_S/IQ4_NL vector-dot samples range from `4.16%` to `4.79%` per worker, while libgomp spin samples range from `3.31%` to `3.88%` for the non-main workers. Sampling does not report the actual number of chunks, dot products, useful execution time, or barrier wait per worker, so it cannot justify a scheduling change.
- Implementation: an opt-in `GGML_MUL_MAT_ID_PROFILE=1` path was added only to the isolated worktree `work/llama.cpp-exp016` and its ignored build directory `build/expert-tier-franken-exp016`. It aggregates per-worker `MUL_MAT_ID` chunk count, dot-product work, useful compute time, and wait time at the immediately following graph barrier, then emits one report at clean process shutdown. It does not log individual chunks. The disabled path has no added synchronization barrier.
- Primary output: per-worker min/median/max and max-to-mean ratios for chunks, dot-product work, compute time, and barrier wait.
- Risks: clock reads and atomic aggregation can perturb the diagnostic run; the aggregate must report its own overhead against an uninstrumented reference. Server requests can have different routing trajectories, so the diagnostic can identify imbalance but not establish a speedup.
- Correctness gate: after removing an initially unnecessary diagnostic-only barrier from the disabled path, `test-backend-ops` passed in the isolated CPU build. The full CUDA build had completed before that final source adjustment; its rebuild and a second full-model confirmation were intentionally stopped at Igor's request. The isolated worktree and all experimental build output are uncommitted and do not affect `build/expert-tier-franken-cuda` or the launcher.

### Results

- One approved diagnostic Qwen request (16 generated tokens, port 8096) produced these per-worker values: chunks `106,617-122,596`; dot products `9,480,620-10,353,772`; useful compute `10.008486-10.252987 s`; immediately following barrier wait `0.315031-0.481208 s`.
- The useful compute spread is only `2.44%` from minimum to maximum. The larger count spread is expected from dynamic chunk acquisition: workers which finish smaller chunks take more work, and it does not translate into a material compute-time imbalance.
- Barrier wait is about `3-5%` of useful `MUL_MAT_ID` compute per worker. Therefore the OpenMP spin samples from EXP-014 are not primarily explained by an imbalanced `MUL_MAT_ID` schedule.
- The diagnostic request was cold and generated a reasoning-only truncated response, so its `6.979 tok/s` is explicitly not compared with the warm baseline and is not a performance result. The server was stopped cleanly after the run; its RSS was about `19.2 GiB` and its process swap was zero.
- Decision: `REJECTED` as a scheduling-optimization direction. Do not alter chunk scheduling. Preserve the findings here; do not merge the experiment-only instrumentation. A future profiling experiment must instead localize graph-level OpenMP waits or evaluate runtime OpenMP wait-policy settings under a controlled warm A/B.

## EXP-2026-09-03-017-openmp-wait-policy

- Status: `REJECTED` as a default profile; `OMP_WAIT_POLICY=PASSIVE` is documented as a manual opt-in mode.
- Hypothesis: OpenMP wait-policy tuning can reduce spin CPU without rewriting the MoE kernel and without losing warm decode speed.
- Bottleneck under test: EXP-014 attributed about `39.96%` of sampled cycles to libgomp pause-based spin waits; EXP-016 excluded `MUL_MAT_ID` chunk imbalance as the cause.
- Change: runtime environment only. The working binary was started with `OMP_WAIT_POLICY=PASSIVE`; no code, model, quantization, context, or thread-count change. The server command otherwise matched the EXP-014/015 reference exactly (12 threads, port 8095, 64K context, q4_0 KV cache, `--reasoning-effort low`), and the variable was verified in `/proc/<pid>/environ`.
- Protocol: one approved gate — a single server start, one warmup 256-token request, one measured 256-token request with 0.5 s `/proc` plus `nvidia-smi` telemetry, compared against the EXP-015 same-setup ON reference. Guardrail: stop at clear degradation above 10%; no formal A/B unless the gate is clean.
- Primary metric: measured warm generation tok/s. Secondary metrics: occupied CPU cores, RSS, GPU utilization, major faults, process swap, and output hash.
- Risks: PASSIVE may cut spin but increase wake-up latency and slow decode; a lower spin-sample count alone would not count as a win without the tok/s comparison.

### Results

- Measured warm decode: `15.72 tok/s` (`16,220.20 ms` per 256 tokens) versus the ON reference `17.914 tok/s`, `-12.25%`; wall time `16.67` versus `14.63 s`. The warmup request took `23.94 s` versus `16.77 s`.
- Mean occupied CPU cores fell from `11.774` to `6.606` (`-43.9%`); peak RSS was `30,002.9` versus `29,469.9 MiB`; mean GPU utilization was `26.5%` versus `29.8%`.
- Correctness and safety passed: the output SHA-256 matched the historical `3be17501f0987ac5881160853f361a274ba40379a14f283b9fa13f61680c11e2`; physical reads, major faults, and process swap were zero.
- Interpretation: sleeping instead of spinning frees about `5.2` cores, which proves the EXP-014 spin cycles are real CPU burn rather than a sampling artifact. The price is a `12.25%` decode loss, so full-passive fails the acceptance gate (noticeable speed loss) and trips the early-stop guardrail.
- Manual mode: `OMP_WAIT_POLICY=PASSIVE` is recorded as an opt-in runtime mode for periods when free cores matter more than single-stream speed, analogous to the retained `THREADS=16` profile. No launcher or code change.
- Decision: `REJECTED` as a default after the early gate. The middle-ground follow-up is conditional EXP-018 with a moderate `GOMP_SPINCOUNT` and no `OMP_WAIT_POLICY`.

## EXP-2026-09-03-018-gomp-spincount

- Status: `REJECTED` (early gate).
- Hypothesis: a moderate `GOMP_SPINCOUNT` — spin briefly, then sleep — captures part of the `5.2`-core CPU saving of EXP-017 without its `-12.25%` decode loss.
- Change: runtime environment only: `GOMP_SPINCOUNT=30000` (one tenth of the libgomp default `300000`), no `OMP_WAIT_POLICY`, working binary, otherwise the identical EXP-014/015/017 server command; the variable was verified in `/proc/<pid>/environ`.
- Protocol: one approved gate — one server start, one warmup 256-token request, one measured 256-token request with 0.5 s `/proc` plus `nvidia-smi` telemetry, against the ON reference `17.914 tok/s` / `11.774` cores.
- Acceptance: `>=3%` speed, or a proven noticeable CPU-core reduction without noticeable speed loss; identical output hash; zero swap.
- Risks: the spin may be load-bearing for barrier latency, in which case even a reduced count degrades decode; only one variable may differ from the reference.

### Results

- Measured warm decode: `17.83 tok/s` (`14,302.61 ms` per 256 tokens) versus the `17.914` reference, `-0.47%`, inside the `+/-2%` paired noise; wall time `14.74 s`; warmup `16.36 s`.
- Mean occupied CPU cores were `11.346` versus `11.774` (`-3.6%`); peak RSS `30,040.0 MiB`; mean GPU utilization `30.4%`.
- Correctness and safety passed: the output SHA-256 matched the historical `3be17501f0987ac5881160853f361a274ba40379a14f283b9fa13f61680c11e2`; physical reads, major faults, and process swap were zero.
- Interpretation: with a still-generous spin budget the workers spin through the typical short graph waits, so neither CPU nor speed changes materially. Together with EXP-015 (native pool) and EXP-017 (PASSIVE) this closes the OpenMP wait-tuning direction: the spin is load-bearing, and its CPU cost can only be removed by sleeping, which costs about `12%` of decode speed.
- Decision: `REJECTED` at the early gate. No default or launcher change. The remaining warm-path options are localizing graph-level waits by separate profiling, or the P5 kernel work where the `iq2_s` and `iq4_nl` dot products hold about `53.7%` of cycles.

## EXP-2026-09-03-019-dot-kernel-microbench

- Status: `ACCEPTED` (survey + measurement); verdict: the plain vec-dot micro-tuning direction is `CLOSED`.
- Hypothesis: the two hot expert dot kernels are either compute-bound (headroom for hand AVX2 tuning) or memory-bound; a single-core micro-benchmark against a read-only stream baseline decides it, per the pre-registered criterion "if the streaming regime runs near the single-core DRAM ceiling, close P5 micro-tuning in favor of P4/P2".
- Bottleneck under test: EXP-014 attributed `53.69%` of sampled cycles to `ggml_vec_dot_iq2_s_q8_K` (`29.19%`) and `ggml_vec_dot_iq4_nl_q8_0` (`24.50%`).
- Survey (step 1, code reading only): both kernels are byte-identical to upstream llama.cpp master and to ik_llama.cpp's plain `vec_dot` implementations; ik_llama.cpp's speed work lives in separate fused `iqk` dequant+GEMM kernels, and Marlin/CUTLASS are CUDA-only. There is no ready-made faster plain vec_dot to port.
- Change: no inference-path change. New standalone `benchmarks/kernel_bench.c` linked against the working build's `libggml-cpu`/`libggml-base`; one pinned core (`taskset -c 8`), `-O3 -march=native`.
- Protocol: per type, cache regimes of 16 blocks (L1/L2), 16384 blocks (L3), and 524288/4194304 blocks (188/208 MiB, above the 64 MiB L3), plus an AVX2 read-sum stream baseline on the same buffers; dot checksums compared against a float reference.
- Benchmark correctness fixes found during the run: `ggml_cpu_init()` is mandatory (the hot loops convert fp16 scales through `ggml_table_f32_f16`, which stays zero without it, so every dot returned exactly `0.0`), and `ggml_quantize_free()` must run only after all dot calls because `vec_dot` also reads the runtime `iq2s_grid` tables. After both fixes the checksums match the float reference at quantization level (`22602.51` vs `22174.54` for iq2_s at n=4096; `2816.22` vs ~`2729` for iq4_nl at n=512) and the timings were unchanged, confirming the earlier zero-checksum runs were timing-valid.

### Results

- iq2_s (374 B/block of combined x+y traffic): `15.46`/`15.52` ns per block in the L1/L2 and L3 regimes (`22.53`/`22.45` GiB/s - a compute plateau, not a bandwidth limit), `19.77` ns per block at the 188 MiB DRAM working set (`17.62` GiB/s) versus `23.69` GiB/s for the read-only stream baseline on the same buffers (`74%` of the single-core stream ceiling). Ideal compute/bandwidth overlap would give ~`15.8` ns per block, so even a perfect rewrite of this kernel is bounded by about `+20%` kernel-time.
- iq4_nl (52 B/block): `1.49` ns/block L1/L2, `1.66` ns/block at 26 MiB, and `2.23` ns/block at a true 208 MiB DRAM working set (`21.76` GiB/s) versus `22.02` GiB/s baseline (`~99%` of the stream ceiling); no headroom in any regime.
- Cross-check against EXP-014 hardware counters: `10.81` billion cache references with only `602.69` million misses (`5.58%`, LLC hit ratio ~`94.4%`) over the 256-token request means ~`2.35` million misses/token (~`147 MiB/token`, ~`2.5 GiB/s` aggregate DRAM traffic) - warm decode does not saturate the machine's dual-channel DDR4. Most in-server kernel invocations therefore run in their L3-resident regime, i.e. at the measured compute plateau, and the earlier "aggregate memory wall" reading is corrected here: the binding constraints are kernel compute (`53.7%` of cycles) plus OpenMP spin (`39.96%`), not aggregate DRAM bandwidth.
- Decision: plain-CPU-kernel micro-tuning is `CLOSED` (ceilings of `+20%`/`+5%` kernel-time cannot translate into a material end-to-end change behind 40% spin overhead). P5 remains reachable only as structurally different kernels (fused dequant+GEMM in the ik_llama.cpp style, or CUDA), and the cheaper near-term levers are the graph-level wait localization left over from P1, then P2/P4.
- Artifacts: `benchmarks/kernel_bench.c`, `results/exp019-kernel-bench.txt`.
