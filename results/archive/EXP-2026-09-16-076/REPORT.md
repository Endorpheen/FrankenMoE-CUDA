# EXP-2026-09-16-076 - asynchronous pinned-source registration: implementation and model-free gate

Status: `MODEL-FREE DONE` - implementation in an isolated tree, build, full model-free gate PASS
including a concurrency stress. No model and no server were run, so no throughput claim is made.
Committed: no.
Hypothesis: `PRE-PIN3`.
Base: `results/archive/EXP-2026-09-16-075/REPORT.md` (EXP-075 pinned-source overlap split).
Isolated tree: `work/llama.cpp-exp076-pin-async`
Build: `build/exp076-pin-async`

## 1. Goal

EXP-075 registered a partly covered request on the request thread. A live A/B of the fixed EXP-075
build measured -43.17 percent on the first request after a fresh server start because that request
paid `cudaHostRegister` for its whole expert set synchronously (`results/archive/EXP-2026-09-16-075/ab2/`).

This experiment takes registration off the request critical path. A request that is not already
fully covered is served by the accepted pinned slot ring (exactly today's default, so there is no
regression) and its page-aligned uncovered remainder is queued; a single background worker pins the
queue under the existing budget, blacklist, overlap and headroom rules. A later request that finds
the range covered takes the direct path.

## 2. Mechanism

Changed in the isolated tree only (exact diff in `pinsrc-async.patch`):

- `work/llama.cpp-exp076-pin-async/ggml/src/ggml-cuda/common.cuh`
- `work/llama.cpp-exp076-pin-async/ggml/src/ggml-cuda/ggml-cuda.cu`

New env `GGML_EXPERT_PIN_ASYNC`, default ON in this build. `0` runs the EXP-075 synchronous split
verbatim. `GGML_EXPERT_PIN_SOURCE` (master switch) and `GGML_EXPERT_PIN_OVERLAP_SPLIT` keep their
meaning when `GGML_EXPERT_PIN_ASYNC=0`. `GGML_EXPERT_PIN_SOURCE_BUDGET_MB` (24576 MiB default) is
unchanged. A new debug env `GGML_EXPERT_PIN_TRACE=1` logs the direct/ring class of every request so
the harness can count it without a model.

Precedence: with async ON the request thread never registers, so async wins over `OVERLAP_SPLIT`.
This is required by the "never register on the critical path when async is ON" rule.

### 2.1 Request path (`ggml_cuda_expert_pin_try_async`)

- The plan is computed under the pin mutex and the copies are issued after it is released.
- If every byte of `[src, src+size)` is inside an existing PHYSICAL registration region, the request
  is copied directly, clamped to each physical region end exactly as EXP-075 does; no registration
  and no queue touch.
- Otherwise the request is not copied at all: the function returns false so the accepted ring serves
  it in full, and each page-aligned uncovered remainder is enqueued. A request therefore either goes
  fully direct within physical regions, or fully through the ring - no byte is copied twice.

### 2.2 Background worker

- One thread, started lazily on the first enqueue, joined in the context destructor inside
  `ggml_cuda_expert_pin_source_release` BEFORE any `cudaHostUnregister`.
- Drains the queue FIFO and registers page-aligned remainders with the same budget (24576 MiB),
  blacklist, overlap, physical-region and 8 GiB `MemAvailable` headroom rules as the sync path.
- The blocking `cudaHostRegister` runs WITHOUT the mutex; the budget is reserved under the mutex
  before the call and rolled back on failure, so a request never waits for a registration.
- Throttling: at most 64 MiB per batch with a 20 ms sleep between batches. The first batch does not
  start until `g_expert_pin_dispatch_done` is set, which happens when the first
  `set_tensor_async_pinned_ring` call of the process returns (a small RAII guard), so the worker does
  not compete with the first request for NVMe and CPU.

### 2.3 Thread safety

One file-static `g_expert_pin_mutex` guards `expert_pin_intervals`, `expert_pin_regions`,
`expert_pin_bytes`, the blacklist, the queue and `expert_pin_state`. The decode thread computes its
copy plan under the mutex and issues the copies after releasing it; that is safe because
registration only ADDS regions and nothing removes one before teardown, and teardown joins the
worker and does `cudaDeviceSynchronize` first. The invariant is stated in a comment next to the mutex.

### 2.4 Queue discipline

- Dedup: a range already covered, already queued or blacklisted is skipped; the worker re-checks
  coverage and the blacklist when it pops a range.
- Bounded capacity `GGML_CUDA_EXPERT_PIN_QUEUE_MAX = 65536` with an explicit drop counter.
- A saturation flag: on the first budget refusal the worker sets `expert_pin_saturated`, drops the
  range and stops dequeuing instead of spinning.

### 2.5 Counters and release log

The release log keeps the EXP-075 prefix and appends worker fields, so the old prefixes still parse:

```
expert pinned source: <direct> direct ranges, <split> split ranges, <fallback> fallback ranges,
<fail> register failures, <intervals> intervals released, <regions> physical regions released,
<bytes> bytes released, <still> bytes still pinned, <sub_direct> covered sub-copies,
<sub_reg> registered sub-copies, <queued> queued ranges, <worker_registered> worker registered
ranges, <dropped> dropped ranges, <dedup> dedup skipped ranges, <batches> worker batches,
<joined> worker joined, <saturated> saturated
```

The worker also logs `expert pin worker: processed N of M queued ranges, queue K` after each batch,
which the harness uses to wait for a drain.

## 3. Build route

The cheap route worked:

- `cp -a work/llama.cpp-exp075-pinsrc-overlap work/llama.cpp-exp076-pin-async`
- `cp -a build/exp075-pinsrc-overlap build/exp076-pin-async`
- The copied CMake cache named the EXP-075 source and binary dirs. Every text file under the copied
  build dir that contained `exp075-pinsrc-overlap` was rewritten to the EXP-076 paths (1192 files),
  then `cmake -S work/llama.cpp-exp076-pin-async -B build/exp076-pin-async` configured cleanly in
  0.5 s. Baseline `ggml-cuda` build 2 min 57 s; after the edit 2 min 53 s; the throttling-log edit
  2 min 52 s; the final dispatch reorder 7.8 s (incremental).

The accepted trees and builds (`work/llama.cpp-integration`, `work/llama.cpp-exp062-ring3`,
`build/exp063-default-runtime`, `work/llama.cpp-exp075-pinsrc-overlap`, `build/exp075-pinsrc-overlap`)
were not modified.

## 4. Model-free gate

Harness `test-exp076-pinsrc.cpp` drives `set_tensor_async_pinned_ring` directly and captures the
release counters through the `ggml_log` callback. The copied unchanged tree was built and gated
first: full-list hash `5bbf98f3...` and scheduler `39505119...` were reproduced before any edit.
All runs are sequential, one process at a time.

### 4.1 (a) Full 17886-range list, async ON, two passes

`exp070_ranges.txt` sha256 `8b58328e...`, 17886 ranges, 23 237 033 984 bytes.

| pass | trace direct | trace ring | hash-list sha256 |
| --- | ---: | ---: | --- |
| pass 1 (ring + enqueue) | 0 | 17886 | `5bbf98f3...` |
| pass 2 (after drain) | 17886 | 0 | `5bbf98f3...` |

Pass 1 enqueues every range and the ring serves it; after the worker reports the queue drained,
pass 2 is served fully by direct copies. The two hash lists are byte-identical and equal the accepted
hash. Release counters: direct 17886, fallback 17886, queued 17886, worker registered 17886,
1199 worker batches, 0 dropped, 0 dedup, worker joined 1, bytes released 23 311 929 344,
bytes still pinned 0, VmSwap 0.

### 4.2 (b) async OFF reproduces EXP-075

`GGML_EXPERT_PIN_ASYNC=0` full-list run: direct 17886, split 0, fallback 0, 17886 intervals,
17886 physical regions, 23 311 929 344 bytes released, worker joined 0, hash `5bbf98f3...`.
The synthetic EXP-075 parity modes also match exactly: selftest direct 5 / split 2,
shifted direct 18 / split 8 / 18 regions, adjacent direct 3 / 2 regions, all checks PASS.

### 4.3 (b') async ON through the same runtime mode

Fallback 17886, worker registered 17880 (6 ranges were still queued when the mode tore the context
down; that mode does not wait for a drain), bytes still pinned 0, worker joined 1, hash `5bbf98f3...`.

### 4.4 (c) register-fail falls back end to end

`GGML_EXPERT_PIN_REGISTER_FAIL=1`, async ON: fallback 17886, register failures 17881,
worker registered 0, bytes released 0, bytes still pinned 0, worker joined 1, hash `5bbf98f3...`.
The worker blacklists each failed interval and never retries it.

### 4.5 (d) Scheduler invariant

`test-exp074-sched.cpp` runs the accepted 395-token MUL_MAT_ID prefill shape through
`ggml_backend_sched`.

| run | output sha256 |
| --- | --- |
| async ON | `39505119b144ea286e865119b397eecff75578881c24c4aa1cec9bd642e91510` |
| `GGML_EXPERT_PIN_ASYNC=0` (split) | `39505119...` |
| `GGML_EXPERT_PIN_ASYNC=0 GGML_EXPERT_PIN_OVERLAP_SPLIT=0` (whole) | `39505119...` |
| `GGML_EXPERT_PIN_REGISTER_FAIL=1` | `39505119...` |

### 4.6 (e) Concurrency stress

`async --iterations 3 --passes 2 --wait-drain --trace`: the full 17886-range list is driven three
times, each pass 1 concurrent with the worker pinning, then the queue is waited out and pass 2 must
be fully direct. 21 PASS / 0 FAIL, no crash; all six hash lists equal `5bbf98f3...`; every iteration
reports worker joined 1, bytes still pinned 0, VmSwap 0. Wall 2 min 39 s.

Saturation was exercised separately with `GGML_EXPERT_PIN_SOURCE_BUDGET_MB=64`: saturated 1,
51 ranges registered (64 786 432 bytes), 17 830 dropped, 5 worker batches, hash `5bbf98f3...`,
no spin. Blacklist dedup was exercised with `REGISTER_FAIL=1` over two passes: 17886 failures,
17886 dedup skips on the second pass, both hash lists `5bbf98f3...`.

### 4.7 (f) Teardown

Every async run reports `worker joined: 1`, `bytes still pinned: 0` and `VmSwap: 0`; the worker is
joined before the first `cudaHostUnregister` in the context destructor.

## 5. Safety notes

- The worker cannot outlive the CUDA context: it is stopped, signalled and joined at the start of
  `ggml_cuda_expert_pin_source_release`, before `cudaDeviceSynchronize` and any `cudaHostUnregister`.
- A request never registers: with async ON the only registration site is the worker.
- No byte is copied twice: a request is either fully direct (inside physical regions) or fully ring.
- The physical-region clamp from EXP-075 is preserved in the direct path, so a `cudaMemcpyAsync`
  never crosses a `cudaHostRegister` end.
- Budget, headroom and blacklist rules are reused unchanged, including the 24576 MiB default and the
  8 GiB `MemAvailable` margin.

## 6. Artifacts

Build (`build/exp076-pin-async/bin/`):

- `llama-server` sha256 `4aec1132ec4febae68e050aa93a58397dcb09feec4e640f130d8cedcb32bdafb` (unchanged)
- `libggml-cuda.so.0.22.0` sha256 `8ecc3a7102aa1c6aea97830eae8d2b198e85bfad7bd94fef13e14a124038d30f`

Gate files in `results/archive/EXP-2026-09-16-076/`:

- `pinsrc-async.patch` - exact diff EXP-075 tree -> EXP-076 tree, sha256 `1c7d093b...`
- `test-exp076-pinsrc.cpp`, `test-exp076-pinsrc` - runtime/async/selftest/shifted/adjacent harness
- `test-exp074-sched.cpp`, `test-exp074-sched` - scheduler-invariant harness
- `baseline-run.log`, `baseline-rev-hash.txt`, `baseline-sched.bin` - pre-edit environment proof
- `a-async-full.log`, `async-full-it1-p{1,2}.txt` - gate (a)
- `b-asyncoff.{log,txt}`, `parity-*-asyoff.log` - gate (b) and EXP-075 parity
- `b-asyncon.{log,txt}` - gate (b')
- `c-regfail.{log,txt}` - gate (c)
- `ringonly.{log,txt}` - master-switch off
- `sat.{log,txt}`, `dedup.{log}`, `dedup-it1-p{1,2}.txt` - saturation and blacklist dedup
- `sched-*.{log,bin}` - gate (d), four configurations
- `e-stress.log`, `e-stress-it{1,2,3}-p{1,2}.txt` - gate (e)
- `build-{baseline,pinasync,workerlog,reorder}.log` - build logs
- `environment.txt` - host, toolchain, binary, harness and patch hashes

## 7. Not covered

- No model and no server were run; no prefill or decode claim is made. The expected benefit is the
  removal of the first-request synchronous registration cost measured at -43.17 percent in EXP-075;
  a live A/B is needed before any claim and is a separate step with Igor's approval.
- The worker competes with the decode thread by design; the throttling constants (64 MiB, 20 ms)
  were chosen to bound the interference, not measured against a model.
- The multi-iteration stress exercises a fresh context per iteration; a single long-lived server
  session (one context, many prompts) is only covered by the single-context logic, not by a model.

## Build note: copied build dirs carry the old RUNPATH

A build directory created with `cp -a build/<old> build/<new>` keeps the `DT_RUNPATH` baked into
every copied executable and shared library. `DT_RUNPATH` is a per-object property, so a copied
`llama-server` still names the OLD `bin` directory in its search path, and the loader resolves
`libggml-cuda.so.0.22.0` (and the other shared libraries) from the OLD build even when the process
is launched from the NEW build directory or with `BUILD_DIR=build/<new>`.

### Defect found in `build/exp076-pin-async`

`build/exp076-pin-async/bin/llama-server` was copied from `build/exp075-pinsrc-overlap` and never
relinked. Its `DT_RUNPATH` still named the EXP-075 `bin`:

```
readelf -d build/exp076-pin-async/bin/llama-server | grep RUNPATH
  RUNPATH [/home/end0/AI/FrankenMoE-CUDA/build/exp075-pinsrc-overlap/bin:]

ldd build/exp076-pin-async/bin/llama-server | grep ggml-cuda
  libggml-cuda.so.0 => /home/end0/AI/FrankenMoE-CUDA/build/exp075-pinsrc-overlap/bin/libggml-cuda.so.0
```

Consequence: a live run started with `BUILD_DIR=build/exp076-pin-async` loaded the EXP-075
`libggml-cuda.so.0.22.0` (sha256 `27137a1a...`) and never executed the EXP-076 async code. The
release-counter field set and the startup log line were the EXP-075 format, lacking the
`, async on` / `, async off` suffix emitted only by the EXP-076 library. The affected run is archived
with a full analysis in `ab-invalid-runpath/INVALID.md` and must not be used as EXP-076 evidence.

Every earlier build (`exp062-ring3`, `exp063-default-runtime`, `exp074-pinsrc`,
`exp075-pinsrc-overlap`) has a correct self-referential RUNPATH; only `exp076` was broken.

### Fix

The affected targets were relinked from the existing CMake object files, using the `link.txt`
command CMake already generated for each target (run with the target binary directory as cwd). The
generated link commands already carried
`-Wl,-rpath,/home/end0/AI/FrankenMoE-CUDA/build/exp076-pin-async/bin:`, so relinking only rewrote
the dynamic link metadata; no source and no object code changed.

Targets relinked: `libggml-cpu.so.0.22.0` (`ggml/src`, `ggml-cpu`), `libggml.so.0.22.0`
(`ggml/src`, `ggml`), `libllama.so.0.3.0` (`src`, `llama`), `libllama-common.so.0.3.0` (`common`,
`llama-common`), `libmtmd.so.0.3.0` (`tools/mtmd`, `mtmd`), `libllama-server-impl.so`
(`tools/server`, `llama-server-impl`), `llama-server` (`tools/server`, `llama-server`).

Example relink for one target (equivalent for every target above, from `build/exp076-pin-async`):

```
cd build/exp076-pin-async/<target binary dir>
sh -c "$(cat CMakeFiles/<target>.dir/link.txt)"
```

`libggml-cuda.so.0.22.0` was not relinked and its sha256 is unchanged:

- before `8ecc3a7102aa1c6aea97830eae8d2b198e85bfad7bd94fef13e14a124038d30f`
- after  `8ecc3a7102aa1c6aea97830eae8d2b198e85bfad7bd94fef13e14a124038d30f`

`llama-server` `.text`, `.rodata` and `.data` are byte-identical to the pre-relink copy; only the
dynamic link metadata changed. Its sha256 moved from `4aec1132...` (pre-relink, line 194 above) to
`8117c2e6aeb8a183a580acd5aecbd13540796f843aa78eab009c35398838b6e2` (relinked).

After the fix every entry under `build/exp076-pin-async/bin/` resolves `libggml-cuda.so.0` from
`build/exp076-pin-async/bin`. The relinked `llama-server`:

```
ldd build/exp076-pin-async/bin/llama-server | grep ggml-cuda
  libggml-cuda.so.0 => /home/end0/AI/FrankenMoE-CUDA/build/exp076-pin-async/bin/libggml-cuda.so.0
```

The async code is now reachable: `strings build/exp076-pin-async/bin/libggml-cuda.so.0.22.0 | grep -c "async %s"` is `1`.

### Required check for every future copied build directory

After `cp -a build/<old> build/<new>` and the CMake reconfigure, verify that the new build resolves
its own libraries before running anything:

```
readelf -d build/<new>/bin/llama-server | grep RUNPATH
ldd       build/<new>/bin/llama-server | grep ggml-cuda
```

`RUNPATH` must name `build/<new>/bin`, and the `ldd` line must resolve `libggml-cuda.so.0` inside
`build/<new>/bin`. If either points at the old build, relink the affected targets as above before
trusting any live result. The same check applies to every executable and shared library in
`build/<new>/bin/`, not only `llama-server`.
