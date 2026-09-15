# D3 - Thread affinity and CPU-placement candidates (ENV/CLI only, EXP-057 design)

Status: DESIGN ONLY. No build, no model/server launch, no repository change.
Scope: the accepted runtime `work/llama.cpp-exp046-default`, launcher
`scripts/run_qwen38_server.sh`, profile `-t 12` (12 CPU threads), `--cpu-moe`,
`-c 196608 -np 1`, MTP `--spec-draft-n-max 2`, EHS=0.
This is a correctness-neutral tuning direction; no speedup is promised.

## 0. Correction to the stated hardware premise

The task states "AMD Ryzen 9 5950X (8 physical cores, 16 threads with SMT)".
The host actually reports 32 logical CPUs / 16 physical cores:

- `nproc` = 32.
- `lscpu -e`: 32 CPUs, `core_id` 0..15, one socket/node.
- `/sys/devices/system/cpu/cpuN/topology/thread_siblings_list`:
  (0,16) (1,17) (2,18) ... (15,31).
  So CPUs 0..15 are the first SMT thread of each physical core; CPUs 16..31 are
  the second SMT thread.

The 5950X is a 16C/32T part. This changes the affinity math:

- 12 threads can be placed on 12 DISTINCT physical cores, leaving 4 physical
  cores (8 logical CPUs) completely idle. There is no need to double up: the
  "12 threads on 8 cores -> 4 cores with 2 threads each" case does not apply
  unless the host were an 8C/16T part (for example the 5800X). If an 8C/16T host
  is really intended, the mask guidance below must be recomputed.
- The measured baseline already spreads the 12 threads over ~12 cores: EXP-013
  reported mean occupied cores 11.95 at `-t 12` (versus 15.93 at `-t 16`).
  So the OS scheduler is already doing most of the placement; explicit affinity
  can only remove the residual migrations and transient SMT co-tenancy, not
  create a 12-core placement that was absent.

## 1. Current thread-placement analysis (from code and defaults)

Defaults are all "off": `common/common.h:68-75`

- `n_threads = -1`
- `cpumask = all false`, `mask_valid = false`
- `priority = GGML_SCHED_PRIO_NORMAL` (0)
- `strict_cpu = false`
- `poll = 50`

Flow of the mask/priority into the compute threads:

- `common/common.cpp:288-310` `postprocess_cpu_params`: an unset role
  (`n_threads < 0`) copies the whole role model, so `--cpu-mask` set only for the
  target is inherited by the batch pool and the draft, unless overridden.
- `common/common.cpp:1794-1808` `ggml_threadpool_params_from_cpu_params`:
  `cpumask` is copied only if `mask_valid`; `priority`, `poll`, `strict_cpu`
  always.
- `common/common.cpp:1818-1860` `common_threadpools::init`: creates the pools and
  attaches them via `llama_attach_threadpool`. If `tpp` and `tpp_batch` match,
  ONE pool is created and reused for both batch and generation; otherwise a
  second (paused) batch pool is created.
- `common/arg.cpp:1555-1642`: `-C/--cpu-mask`, `-Cr/--cpu-range`,
  `--cpu-strict`, `--prio`, `--poll`, and the `-b`/`-batch` variants.
  `common/arg.cpp:4090-4175`: the `--spec-draft-*` draft variants.
- `ggml/src/ggml-cpu/ggml-cpu.c:2988-3008` `ggml_thread_cpumask_next`:
  - `strict_cpu = false` (default): every worker gets a copy of the FULL mask.
    The mask only confines; the kernel still schedules/migrates inside it.
  - `strict_cpu = true`: worker j gets exactly one set bit, chosen round-robin
    through the global mask, wrapping modulo `GGML_MAX_N_THREADS`. With 12 set
    bits and 12 threads each thread gets a distinct CPU.
- `ggml/src/ggml-cpu/ggml-cpu.c:3683-3702`: under `GGML_USE_OPENMP` the graph
  runs in `#pragma omp parallel num_threads(n_threads)` and each OpenMP worker
  applies priority and its per-worker affinity (`ggml_thread_apply_affinity`,
  Linux implementation at `ggml-cpu.c:2913-2940` via `pthread_setaffinity_np`).
  So `--cpu-mask`/`--cpu-strict` DO take effect on this build.
- Build confirmation: `build/exp046-default-runtime/CMakeCache.txt` has
  `GGML_OPENMP:BOOL=ON`; the OpenMP path is live.

Two important consequences found from the source:

1. `--poll` is INERT on this build. The polling loop
   `ggml_graph_compute_poll_for_work` and its readers are compiled only under
   `#ifndef GGML_USE_OPENMP` (`ggml-cpu.c:3440`, `:3472-3485`; the OMP branch is
   `:3683-3706`). The real wait knob is libgomp's spin, which was already closed
   by EXP-017/018/020. Do not spend runs on `--poll`/`--poll-batch`/`--poll-draft`.
2. The draft CPU masks are INERT in the server path. The target context gets its
   threadpool from `common_init_from_params` (`tools/server/server-context.cpp:1100`)
   and `common_threadpools::init`. The draft context is created directly with
   `llama_init_from_model` (`common/speculative.cpp:2613` and `:2625`) and no
   threadpool is attached to it. When the draft computes, `llama_context::graph_compute`
   (`src/llama-context.cpp:2554-2566`) sets the CPU backend's threadpool to the
   draft context's `threadpool`, which is null, so ggml builds a disposable
   threadpool from default params (`ggml_threadpool_params_default`) with an EMPTY
   cpumask. The draft thread COUNT still works (`-td` sets `cparams.n_threads`),
   which is why EXP-047 saw an effect, but `--spec-draft-cpu-mask/-range/-strict`
   have no pool to bind. Treat them as no-ops for this deliverable.

Net current placement: no mask, no strict, normal priority; 12 OpenMP workers
free to run on any of the 32 logical CPUs under the CFS scheduler; EXP-014 saw
IPC 1.159 and libgomp spin/wait at ~39.96% of sampled cycles. The mask direction
is about constraining that placement, not about adding parallelism.

## 2. Candidates

Format: flag delta | mechanism | expected effect | risk | how to measure.
Gate A = decode256 request, gate P = prefill/tail, gate J = jitter.

### C1 - Strict pin to 12 distinct physical cores (PRIMARY)
- Delta: `--cpu-range 0-11 --cpu-strict 1`
- Mechanism: `strict_cpu` gives worker j exactly one CPU from bits 0..11
  (`ggml-cpu.c:2988-3008`), so the 12 workers land on the 12 distinct physical
  cores of core_id 0..11, never on an SMT sibling (siblings are 16..31) and
  never migrate. The 4 remaining physical cores (12..15 and all siblings) stay
  free for the HTTP thread, the staging/CUDA submission main thread, the desktop
  and the GPU driver.
- Expected: decode may be neutral to slightly positive; the main expected win is
  less jitter (fewer migrations, no transient two-workers-on-one-core) and a
  cleaner CPU footprint. Do not expect >=2% from placement alone.
- Risk: low. A pinned worker cannot be migrated to an idle core if it is briefly
  descheduled, so a slow MUL_MAT_ID chunk cannot be offloaded; per-call
  `pthread_setaffinity_np` adds 12 syscalls per graph (negligible at ~100 ms/step).
- Measure: A (decode), J. Record per-request tok/s and the content hash.

### C2 - Confinement only (no pinning)
- Delta: `--cpu-range 0-11` (leave `--cpu-strict` at 0)
- Mechanism: all 12 workers share the same 0..11 mask; the kernel may still
  migrate them inside the set but cannot place them on siblings 16..31 or on
  cores 12..15.
- Expected: isolates the "mask-only" effect from the "pinning" effect of C1.
  Likely neutral versus baseline.
- Risk: very low.
- Measure: A, J.

### C3 - Second SMT half (topology control)
- Delta: `--cpu-range 16-27 --cpu-strict 1`
- Mechanism: pins to the second sibling of cores 0..11, leaving CPUs 0..15 (all
  first siblings) free for everything else.
- Expected: should match C1 if SMT half does not matter; a difference would show
  a real first/second-sibling or OS-noise asymmetry. This is a control, not a
  performance candidate.
- Risk: low.
- Measure: A, J.

### C4 - Offset 12-core window (interference control)
- Delta: `--cpu-mask 0xfff0 --cpu-strict 1` (bits 4..15 = cores 4..15)
- Mechanism: same as C1 but avoids cores 0..3, where the kernel, IRQ/GPU and the
  desktop tend to run; keeps interference away from the inference set.
- Expected: neutral; useful if the machine is not idle and low cores are busy.
- Risk: low.
- Measure: A, J.

### C5 - Process-wide confinement (external CLI, optional)
- Delta: prefix the launcher/binary with `taskset -c 0-15` (first siblings of all
  16 cores), optionally combined with C1.
- Mechanism: confines the WHOLE process (HTTP thread, staging/main thread, CUDA
  helper threads) to CPUs 0..15, so nothing is ever scheduled on the second SMT
  half; C1 then pins the 12 OMP workers inside that set.
- Expected: neutral for decode, but it removes OS placement of the process onto
  the high siblings; may reduce jitter if other processes compete.
- Risk: low-medium: it is an external tool (not a llama.cpp flag), affects CUDA
  helper threads, and 0-15 is the full first-sibling set so it is not a subset of
  C1's 0-11. Keep as an optional second stage.
- Measure: A, J.

### P1 - Batch pool thread count for prefill (`-tb`)
- Delta: `--threads 12 --threads-batch 16` plus
  `--cpu-range-batch 0-15 --cpu-strict-batch 1`.
  (Batch flags set explicitly because `postprocess_cpu_params` would otherwise
  copy the 12-bit mask and warn "not enough set bits" for 16 threads
  `common/common.cpp:306-309`.)
- Mechanism: prefill (`batched=true`) uses the batch pool; decode uses the
  12-thread generation pool. A larger batch pool may speed the CPU gather that
  dominates the long prompt (EXP-043: gather-bound, 12 threads).
- Expected: possible prefill gain; decode unchanged because the batch pool is
  paused (`common.cpp:1847-1849`). EXP-013 showed 16 threads do not help decode,
  but that was decode, not the gather.
- Risk: medium: a second threadpool is created; 16 threads on 16 physical cores
  is safe, but SMT was rejected at 24/32 for decode. Correctness is unaffected.
- Measure: P (bulk prefill latency), A (decode must not regress).

### P2 - Ubatch / batch shape (`-ub`, `-b`)
- Delta examples: `-ub 256`, `-ub 1024`; `-b 512`.
- Mechanism: `-ub` sets the physical prefill chunk (`common/common.h:451-452`,
  default `-b 2048`, `-ub 512`). Decode and MTP verify batches are 2-3 tokens,
  far below the 32-token minimum, so `-ub` CANNOT change decode shapes. The only
  lever is prefill and, through the checkpoint break `{4 + n_ubatch, 4}` seen in
  EXP-043, the size of the CPU 4-token tail. `-b` only caps the logical batch and
  is irrelevant to a 395-token prompt while `-b` stays above it.
- Expected: changes prefill latency and the tail, not decode/jitter.
- Risk: correctness-adjacent: llama.cpp does not guarantee bit-identical results
  across batch shapes (server README, `cache_prompt` note) and CUDA graph shapes
  may change. Byte-identical output must be re-verified. This direction is out of
  scope for a decode/jitter test and should only be opened with a prefill goal.
- Measure: P, plus an explicit output-hash and draft-acceptance check.

### PRIO - Priority (`--prio`)
- Delta: `--prio -1` (SCHED_BATCH) as the only low-risk value; `--prio 1..3`
  (SCHED_FIFO 40/80/90) are the aggressive values.
- Mechanism: `ggml-cpu.c:3697` applies the priority to every OpenMP worker each
  region; `ggml-cpu.c:2942-2965` maps LOW->SCHED_BATCH, others->SCHED_FIFO.
- Expected: SCHED_BATCH may slightly reduce preemption/migration of the workers;
  SCHED_FIFO could cut barrier latency but only if permitted.
- Risk: SCHED_FIFO needs CAP_SYS_NICE (otherwise
  `pthread_setschedparam` fails with EPERM and only warns), and realtime workers
  can starve the HTTP thread, the swap watchdog, the monitor and the GPU driver.
  Do not use `--prio >=1` on the daily machine.
- Measure: A, J; check the log for the "failed to set thread priority" warning.

### OMP - OpenMP environment (note only, do not reopen)
EXP-015 (`GGML_OPENMP=OFF`), EXP-017 (`OMP_WAIT_POLICY=PASSIVE`, -12.25% decode)
and EXP-018 (`GOMP_SPINCOUNT=30000`, -0.47%) closed the wait-tuning direction;
EXP-020 localized the spin to distributed graph barriers. Affinity changes the
side cost, not the load-bearing nature of the spin: with C1 the spinning is
confined to cores 0..11 and no longer leaks onto siblings/cores 12..15, so the
CPU-saving argument for PASSIVE is weaker, not stronger. Do not set
`OMP_PROC_BIND`/`OMP_PLACES`/`GOMP_CPU_AFFINITY` together with `--cpu-mask`;
ggml re-binds each worker per region and the two mechanisms would fight. The only
OMP action considered here is: leave all OMP_* unset.

## 3. Measurement protocol (manual, one run at a time)

Per arm (A and B):
1. Start one fresh server. Wait for "server is listening" in the log.
2. Warmup: one `request-short.json`
   (`results/archive/EXP-2026-09-07-040/ab/request-short.json`).
3. Measured: `request-decode256.json`
   (`results/archive/EXP-2026-09-09-046/request-decode256.json`,
   sha256 `5975261b...`, temperature 0, n_predict 256, cache_prompt false,
   stream false). Read `timings.predicted_per_second` and
   `timings.predicted_per_token_ms`.
4. Jitter: repeat the same measured request 2 more times in the SAME server
   session (3 samples), use the sample standard deviation of
   `predicted_per_second`. Do not change requests between samples.
5. Correctness/controls (every run): 256/256 stop=limit; content sha256 identical
   between arms (expected `9270353f0601d5d6...`); draft acceptance identical to
   the digit (EXP-046: 0.58974, 138/234); expert ring `25326/25326`; VmSwap=0;
   clean SIGINT, no orphan.
6. Optional migration evidence (one request per arm, manual):
   `perf stat -e task-clock,context-switches,cpu-migrations -p <server_pid> -- sleep 20`
   while a measured request runs. Compare the cpu-migrations counts; do not tune
   on the perf overhead run, use the plain run for tok/s.

Balance the order (A->B and B->A) and never run two servers at once. All
launches and any commit require Igor's explicit approval (AGENTS.md rule 1/2).

## 4. Recommended first A/B (minimal run count)

Test C1 versus baseline A only. Do not test C2-C5, P1, P2 or PRIO before C1 is
resolved.

Stage 1 (screen): 4 runs = 2 balanced pairs, order A,B,B,A; one warmup + one
measured request per run; 3 jitter samples per run.
- Stop and record REJECTED if the median paired decode gain is < +0.5% AND the
  jitter standard deviation improves by < 20%.
- Escalate if the screening gate is met.

Stage 2 (formal): 5 pairs = 10 runs, balanced 3 A->B + 2 B->A or the reverse.
- ACCEPT if the median paired decode gain is >= +2.0% AND >=4/5 pairs win, or if
  jitter is reduced >= 20% with a decode regression <= 0.5%, and every
  correctness control above passes byte-for-byte.
- Otherwise REJECTED with the numbers recorded.

Rationale for 4 + 10: placement alone is unlikely to reach +2%, so a cheap
screen avoids burning 10 runs on a null; the 5-pair stage matches the repository
decision rule if a real signal appears.

Exact launch lines (DO NOT execute in this task):

    cd /home/end0/AI/FrankenMoE-CUDA

    # Arm A (baseline, 12 threads, no mask)
    PORT=8081 \
    LOG_PATH=results/exp057-A.log \
    MONITOR_PATH=results/exp057-A-monitor.csv \
    scripts/run_qwen38_server.sh \
      models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf

    # Arm B (C1: 12 threads pinned to 12 distinct physical cores)
    PORT=8081 \
    EXTRA_ARGS="--cpu-range 0-11 --cpu-strict 1" \
    LOG_PATH=results/exp057-B.log \
    MONITOR_PATH=results/exp057-B-monitor.csv \
    scripts/run_qwen38_server.sh \
      models/qwen38/UD-IQ3_XXS/Qwen3.8-Flash-Next-UD-IQ3_XXS-00001-of-00003.gguf

    # after "server is listening" in the log: warmup
    curl -s http://127.0.0.1:8081/completion \
      -H 'Content-Type: application/json' \
      -d @results/archive/EXP-2026-09-07-040/ab/request-short.json \
      | jq -r '.timings.predicted_per_second'

    # measured decode256, capture tok/s, per-token ms and content hash
    curl -s http://127.0.0.1:8081/completion \
      -H 'Content-Type: application/json' \
      -d @results/archive/EXP-2026-09-09-046/request-decode256.json \
      | tee results/exp057-A-decode256.json \
      | jq -r '"\(.timings.predicted_per_second)  \(.timings.predicted_per_token_ms)"'
    jq -r '.content' results/exp057-A-decode256.json | sha256sum

    # stop the server cleanly
    kill -INT <server_pid>

The launcher defaults are unchanged: `THREADS=12`, `-c 196608`, `-np 1`, `-fa on`,
KV q4_0, `--cpu-moe`, MTP `n_max=2`, EHS=0 (`scripts/run_qwen38_server.sh:21,48,51,58,86`).
The only A/B variable is `EXTRA_ARGS`; `EXTRA_ARGS` is expanded unquoted at
line 86, so the two flags are passed verbatim.

## 5. What to record for the verdict

- Per run: arm, order, tok/s, per-token ms, 3 jitter samples, content sha256,
  draft acceptance, ring counters, VmSwap, exit status.
- Per pair: 100*(B/A - 1) on decode tok/s; jitter ratio.
- Summary: median paired gain, wins/total, independent arm medians, and an
  explicit limit: this is a placement experiment, not evidence of a kernel or
  memory win.

## 6. Overall risk statement

Low. All candidates are runtime flags; no source, model, quantization, KV or
launcher change. Correctness must still be re-proven byte-for-byte per run.
The realistic expectation is a neutral decode result with a possible jitter
improvement; if C1 is null, the whole affinity direction should be recorded as
REJECTED and closed, with the OMP wait direction staying closed.
