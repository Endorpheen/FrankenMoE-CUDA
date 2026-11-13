# EXP-2026-09-17-077 - idle-gated pin worker: implementation and model-free gate

Status: `MODEL-FREE DONE`. No model, no server and no `curl` was run. No throughput claim is made.
Committed: no (this session does not commit).
Hypothesis: `PRE-PIN3b` (make the first requests neutral against the ring arm).
Base: `results/archive/EXP-2026-09-16-076/REPORT.md` (EXP-076 asynchronous pinned-source registration).
Isolated tree: `work/llama.cpp-exp077-pin-async2`
Build: `build/exp077-pin-async2`

## 1. Goal

EXP-076 moved registration off the request thread, but released the worker when the FIRST
`set_tensor_async_pinned_ring` call returned, i.e. in the middle of the first prefill. The worker
then pinned about 24 GiB while the first requests were still running and fought them for NVMe and
CPU. The EXP-076 long-session A/B measured prompt 1 at `-65.3 percent` and prompt 2 at
`-18.8 percent`, while prompts 3 to 12 were faster in 10/10 (median `+15.0 percent`).

This experiment holds the worker back until the request stream is quiet (idle gate) and makes the
worker gentler (smaller batches, longer sleeps). The expected effect is that prompts 1 and 2 stop
losing to the ring arm, while the later-request win stays. This session only builds the change and
runs the model-free gate; the live re-measurement is a separate step.

## 2. Mechanism

Changed in the isolated tree only (exact diff in `pinsrc-idlegate.patch`, sha256
`0e46ddf45aa105e54225c3c10a6435abb0e8b81326ab44e522083f45c79f46b`):

- `work/llama.cpp-exp077-pin-async2/ggml/src/ggml-cuda/common.cuh`
- `work/llama.cpp-exp077-pin-async2/ggml/src/ggml-cuda/ggml-cuda.cu`

Everything from EXP-076 is kept: `GGML_EXPERT_PIN_ASYNC` default ON, `0` reproduces EXP-075, no
registration on the request thread under async, a request is either fully direct inside physical
regions or fully through the ring, the physical-region clamp, and the worker joined before any
`cudaHostUnregister`.

### 2.1 Idle gate (new)

The worker now needs TWO conditions before it starts a batch (both documented in one comment at the
wait predicate):

1. the first-dispatch guard from EXP-076: the first `set_tensor_async_pinned_ring` call of the
   process must have returned, so the worker never starts before the request path was exercised;
2. the idle gate: `now - last_enqueue_ts >= GGML_EXPERT_PIN_IDLE_MS` (default 750 ms), so the worker
   does not compete with an active request stream.

`ggml_cuda_expert_pin_enqueue_locked` stamps `expert_pin_last_enqueue` on every enqueue (and
`expert_pin_first_enqueue` on the first one), under the pin mutex. The worker's wait predicate checks
the idle condition in the `ggml_cuda_expert_pin_worker_run` block, tree lines 2923 to 2958:

```
// Two conditions must hold before a batch starts:
// 1. the first set_tensor_async_pinned_ring call of the process has returned, ...
// 2. the request thread has not enqueued anything for GGML_EXPERT_PIN_IDLE_MS, ...
```

A deferral is counted once per arrival at the gate (not per wakeup). While the worker sits in the
idle wait it wakes on its own timeout, so `ggml_cuda_expert_pin_try_async` skips `notify_one` when
`expert_pin_idle_waiting` is set; that avoids one wakeup per enqueued range during a long prefill.
`GGML_EXPERT_PIN_IDLE_MS=0` disables the gate and restores the EXP-076 start behaviour (the
first-dispatch guard still applies).

### 2.2 Gentler throttle (new defaults, env-overridable)

| knob | EXP-076 | EXP-077 default | env override |
| --- | ---: | ---: | --- |
| batch cap | 64 MiB | 4 MiB | `GGML_EXPERT_PIN_BATCH_MB` |
| inter-batch sleep | 20 ms | 100 ms | `GGML_EXPERT_PIN_BATCH_SLEEP_MS` |

Both are read once through cached env helpers (`ggml_cuda_expert_pin_batch_bytes`,
`ggml_cuda_expert_pin_batch_sleep_ms`) so a live A/B can sweep them without a rebuild. The queue
bound `GGML_CUDA_EXPERT_PIN_QUEUE_MAX = 65536` is unchanged.

### 2.3 New counters

- `idle deferrals`: times the idle gate held a ready batch back.
- `first batch delay`: measured from the first enqueue to the start of the first batch.

Both are logged on a dedicated worker line
(`expert pin worker: first batch after <x> ms (idle <n> ms, deferrals <k>)`) and appended to the
release summary, whose field count grew from 17 (EXP-076) to 19. The harness parses the longest
format first, so the EXP-075 and EXP-076 prefixes still parse.

## 3. Build route

- `cp -a work/llama.cpp-exp076-pin-async work/llama.cpp-exp077-pin-async2`
- `cp -a build/exp076-pin-async build/exp077-pin-async2`
- Every text file under the copied build dir that contained `exp076-pin-async` was rewritten to
  `exp077-pin-async2` (1167 files), then `cmake -S work/llama.cpp-exp077-pin-async2 -B
  build/exp077-pin-async2` reconfigured cleanly in 0.6 s.
- Baseline `ggml-cuda` build (unchanged EXP-076 source): 3 min 3 s. After the edit: 3 min 11 s
  (changing `common.cuh` rebuilds all CUDA translation units).
- The accepted trees and builds (`work/llama.cpp-integration`, EXP-076, EXP-075, EXP-074 and their
  builds) were not modified. Their `libggml-cuda.so.0.22.0` hashes are unchanged (section 6).

Sequential builds only; the model-free gate ran one process at a time.

## 4. Model-free gate

Harness `test-exp077-pinsrc.cpp` extends the EXP-076 harness: it parses the 19-field release log,
captures the first-batch delay, and adds an `idle` mode. The copied unchanged EXP-076 source was
built and gated FIRST: full-list hash `5bbf98f3...` and scheduler `39505119...` were reproduced
before any edit (`baseline-rev-hash.txt`, `baseline-sched.bin`).

### 4.1 (a) Full 17886-range list, async ON, defaults, two passes

`exp070_ranges.txt` sha256 `8b58328e...`, 17886 ranges, 23 237 033 984 bytes. Defaults: 4 MiB batch,
100 ms sleep, 750 ms idle.

| pass | trace direct | trace ring | hash-list sha256 |
| --- | ---: | ---: | --- |
| pass 1 (ring + enqueue) | 0 | 17886 | `5bbf98f3...` |
| pass 2 (after drain) | 17886 | 0 | `5bbf98f3...` |

Pass 1 enqueues every range and the ring serves it; the worker stays idle for the whole pass
(first batch after 30355.9 ms, i.e. after pass 1 ended plus the idle window). The harness then waits
for the drain and pass 2 is fully direct. Release counters: direct 17886, fallback 17886, queued
17886, worker registered 17886, 4593 worker batches, 0 dropped, 0 dedup, worker joined 1, bytes
released 23 311 929 344, bytes still pinned 0, VmSwap 0, idle deferrals 1. The two hash lists are
byte-identical and equal the accepted hash. Wall 8 min 44 s (dominated by the 4 MiB / 100 ms drain).

### 4.2 (b) async OFF reproduces EXP-075

`GGML_EXPERT_PIN_ASYNC=0` full-list run (`runtime` mode): direct 17886, split 0, fallback 0, 17886
intervals, 17886 physical regions, 23 311 929 344 bytes released, worker joined 0, hash
`5bbf98f3...`. The synthetic EXP-075 parity modes also match exactly: selftest direct 5 / split 2,
shifted direct 18 / split 8 / 18 regions, adjacent direct 3 / 2 regions, all checks PASS.
The 19-field release parser reads the async-OFF counters correctly (new fields zero).

### 4.3 (b') async ON through the `runtime` mode (no drain wait)

Fallback 17886, worker registered 0, idle deferrals 1, worker joined 1, bytes still pinned 0, hash
`5bbf98f3...`. This is the direct signature of the idle gate: the request stream runs for the whole
mode, so the worker never opens a batch and never competes, unlike EXP-076 which registered 17880
ranges in the same mode.

### 4.4 (c) register-fail falls back end to end

`GGML_EXPERT_PIN_REGISTER_FAIL=1`, async ON, async mode with drain. The worker opens after the idle
gate and fails every registration: fallback 35772 (2 passes), register failures 17886, worker
registered 0, bytes released 0, bytes still pinned 0, dedup 17886 on the second pass, worker joined
1, hash `5bbf98f3...`. The `post-drain pass fully direct` assertion is not applicable here (nothing
registers) and the run is driven with `--no-direct-assert`; the hash anchor still holds.

### 4.5 (d) Scheduler invariant

`test-exp074-sched` runs the accepted 395-token MUL_MAT_ID prefill shape through
`ggml_backend_sched`.

| run | output sha256 |
| --- | --- |
| async ON | `39505119b144ea286e865119b397eecff75578881c24c4aa1cec9bd642e91510` |
| `GGML_EXPERT_PIN_ASYNC=0` (split) | `39505119...` |
| `GGML_EXPERT_PIN_ASYNC=0 GGML_EXPERT_PIN_OVERLAP_SPLIT=0` (whole) | `39505119...` |
| `GGML_EXPERT_PIN_REGISTER_FAIL=1` | `39505119...` |

### 4.6 (e) Concurrency stress with drain

`async --iterations 3 --passes 2 --wait-drain --trace` with `BATCH_MB=64 BATCH_SLEEP_MS=20` (the
throttle is orthogonal to the concurrency invariant and keeps the gate tractable; the new defaults
are proven in (a)). 21 PASS / 0 FAIL, no crash; all six hash lists equal `5bbf98f3...`; every
iteration reports worker registered 17886, worker joined 1, bytes still pinned 0, VmSwap 0.

A second variant restores the EXP-076 concurrent start with `GGML_EXPERT_PIN_IDLE_MS=0`: 7 PASS /
0 FAIL, worker registered 17886, worker joined 1. This proves the new code still pins concurrently
with an active request stream when the gate is disabled.

### 4.7 (f) Supplementary: budget saturation

`BUDGET_MB=64 BATCH_MB=64 BATCH_SLEEP_MS=20`: saturated 1, 51 ranges registered (64 786 432 bytes),
17 836 dropped, 1 worker batch, both hash lists `5bbf98f3...`, no spin. The drain wait is not
applicable under saturation (the worker stops by design and the queue never empties), so that one
harness check reports FAIL; the saturation counters and the hash anchor are correct.

### 4.8 Idle gate case (new)

`idle --max-ranges 1000` with defaults (idle 750 ms, 4 MiB, 100 ms). A continuous stream of 1000
ranges ran for 1818.7 ms with a maximum inter-enqueue gap of 28.5 ms (well under the 750 ms
threshold). Results:

- worker issued NO batch while the stream was active (batches during stream = 0);
- first batch at 2538.0 ms measured from the first enqueue = stream (1818.7 ms) plus the idle window
  (750 ms) minus the last range's readback; release counter `first batch delay ms` = 2538;
- idle deferrals = 1;
- after the stream went quiet the worker drained (299 batches, 1000 ranges registered);
- the second pass was fully direct (1000/1000) and both hash lists are byte-identical
  (`b5b2a9dc...`), VmSwap 0.

## 5. Build integrity: copied build dir RUNPATH

`cp -a build/<old> build/<new>` carries the old `DT_RUNPATH` inside every copied executable and
shared library. EXP-076 was shipped with `llama-server` loading `build/exp075-pinsrc-overlap/bin/
libggml-cuda.so.0.22.0`, which silently invalidated its first live A/B.

After the copy, 7 targets still named `build/exp076-pin-async/bin`. Each was relinked from the
existing CMake object files, using the CMake-generated `CMakeFiles/<target>.dir/link.txt` run with
the target binary directory as cwd (`relink.log`). The generated link commands already carried
`-Wl,-rpath,/home/end0/AI/FrankenMoE-CUDA/build/exp077-pin-async2/bin:`, so relinking only rewrote
the dynamic link metadata; no source and no object code changed.

Targets relinked: `libggml-cpu.so.0.22.0`, `libggml.so.0.22.0`, `libllama.so.0.3.0`,
`libllama-common.so.0.3.0`, `libmtmd.so.0.3.0`, `libllama-server-impl.so`, `llama-server`.

`runpath-sweep.txt` (full table):

| file | RUNPATH inside own bin | `ldd` libggml-cuda.so.0 |
| --- | --- | --- |
| `libggml-base.so.0.22.0` | n/a (no RUNPATH) | not linked |
| `libggml-cpu.so.0.22.0` | yes | not linked |
| `libggml-cuda.so.0.22.0` | yes | not linked (itself) |
| `libggml.so.0.22.0` | yes | `build/exp077-pin-async2/bin/libggml-cuda.so.0` |
| `libllama-common.so.0.3.0` | yes | `build/exp077-pin-async2/bin/libggml-cuda.so.0` |
| `libllama-server-impl.so` | yes | `build/exp077-pin-async2/bin/libggml-cuda.so.0` |
| `libllama.so.0.3.0` | yes | `build/exp077-pin-async2/bin/libggml-cuda.so.0` |
| `libmtmd.so.0.3.0` | yes | `build/exp077-pin-async2/bin/libggml-cuda.so.0` |
| `llama-server` | yes | `build/exp077-pin-async2/bin/libggml-cuda.so.0` |

Marker: `strings build/exp077-pin-async2/bin/libggml-cuda.so.0.22.0 | grep -c "async %s"` = 1 (the
EXP-075 library = 0). The EXP-077 async/idle code is therefore reachable.

## 6. Artifacts

Build (`build/exp077-pin-async2/bin/`):

- `llama-server` sha256 `025705256503cd47cae78f9f431c29b7ba4205cea86c91b969ec92453958044c`
- `libggml-cuda.so.0.22.0` sha256 `5edac915efe5cafd52ecaf63b2c467a4eff9d47e9e8cc25fbb292722ac50d5d5`

Reference builds, untouched (see `environment.txt`):

- `build/exp076-pin-async/bin/libggml-cuda.so.0.22.0` `8ecc3a71...`
- `build/exp075-pinsrc-overlap/bin/libggml-cuda.so.0.22.0` `27137a1a...`
- `build/exp074-pinsrc/bin/libggml-cuda.so.0.22.0` `36dba1b2...`

Gate files in `results/archive/EXP-2026-09-17-077/`:

- `pinsrc-idlegate.patch` sha256 `0e46ddf4...` - exact diff EXP-076 tree -> EXP-077 tree
- `test-exp077-pinsrc.cpp`, `test-exp077-pinsrc`; `test-exp074-sched.cpp`, `test-exp074-sched`
- `baseline-run.{out,log}`, `baseline-rev-hash.txt`, `baseline-rev-hash-p2.txt`,
  `baseline-sched.bin`, `baseline-sched-{asyncON,asyncOFF,splitoff,regfail}.{log,bin}` - pre-edit proof
- `a-async-full.{out,log}`, `a-async-full-it1-p{1,2}.txt` - gate (a), defaults
- `b-asyncoff.{log,txt}`, `b-asyncon.{log,txt}`, `ringonly.{log,txt}` - gates (b) and (b')
- `parity-{selftest,shifted,adjacent}-asyoff.log` - EXP-075 parity
- `c-regfail.{out,log}`, `c-regfail-it1-p{1,2}.txt` - gate (c)
- `sched-{asyncON,asyncOFF,splitoff,regfail}.{log,bin}` - gate (d)
- `e-stress.{out,log}`, `e-stress-it{1,2,3}-p{1,2}.txt` - gate (e)
- `e2-concurrency-idle0.{out,log}`, `e2-concurrency-idle0-it1-p{1,2}.txt` - IDLE_MS=0 variant
- `f-sat.{out,log}`, `f-sat-it1-p{1,2}.txt` - supplementary saturation
- `d-idle.{out,log}`, `d-idle-p{1,2}.txt` - idle-gate case (new)
- `runpath-sweep.txt`, `relink.log` - build integrity
- `build-baseline.log`, `build-idlegate.log` - build logs
- `environment.txt` - host, toolchain, binary, harness, patch and ranges hashes

## 7. Not covered

- The live A/B is NOT re-run here. This session produces a build and a model-free gate only, and
  makes no throughput claim. Prompts 1 and 2 must be re-measured in a cache-balanced order (4 pairs,
  two ring-then-async, two async-then-ring) before any session verdict.
- The idle threshold 750 ms, the 4 MiB batch cap and the 100 ms sleep are bounded guesses, not
  measurements. They are env-overridable precisely so the live A/B can sweep them.
- The model-free gate drives the backend interface directly; it does not prove the server's request
  scheduling produces quiet windows long enough for the gate to open between prompts. That is what
  the live A/B must confirm.
- The worker still competes with the decode thread when a request stream finally goes quiet; the
  gentler throttle only bounds that competition.

## 8. Open follow-up

Live A/B of `build/exp077-pin-async2` against the accepted ring arm, balanced order, prompts 1 and 2
as the decision points. If prompts 1 and 2 are neutral while prompts 3+ keep the EXP-076 win, the
idle-gated worker becomes the candidate default.
