# EXP-2026-09-17-078 - idle gate kept, throttle removed: fast pin worker + bounded yield

Status: `MODEL-FREE DONE`. No model, no server and no `curl` was run. No throughput claim is made.
Committed: no (this session does not commit).
Hypothesis: `PRE-PIN3c` (remove the worker throttle, keep the idle gate, yield when requests resume).
Base: `results/archive/EXP-2026-09-17-077/REPORT.md` (EXP-077 idle-gated asynchronous registration).
Isolated tree: `work/llama.cpp-exp078-pin-idlefast`
Build: `build/exp078-pin-idlefast`

## 1. Goal

EXP-077 kept the idle gate but capped the worker at 4 MiB per batch with a 100 ms sleep. A balanced
12-prompt live A/B then measured a NULL session result (-1.6 percent) because the throttle pinned
only about 1.2 GiB in the quiet windows instead of the about 24 GiB a session needs. The gate itself
worked: the first batch started 4.4 to 6.2 s in, with one idle deferral.

This experiment removes the throttle to the aggressive end (64 MiB per batch, 0 ms sleep) while
keeping the idle gate exactly as EXP-077, and adds a bounded yield so the worker closes a batch as
soon as a request resumes. The point is to measure, model-free and the same way for both settings,
how fast the worker pins during a quiet window and how much of a starting request it touches.

## 2. Mechanism

Changed in the isolated tree only (exact diff in `pin-idlefast.patch`, sha256
`505d89071a049adacf9439568c93f0cb9e86be072ba86d1b0f01825a7c292064`):

- `work/llama.cpp-exp078-pin-idlefast/ggml/src/ggml-cuda/common.cuh`
- `work/llama.cpp-exp078-pin-idlefast/ggml/src/ggml-cuda/ggml-cuda.cu`

Everything from EXP-077 is kept: `GGML_EXPERT_PIN_ASYNC` default ON, `0` reproduces EXP-075, no
registration on the request thread under async, a request is either fully direct inside physical
regions or fully through the ring, the physical-region clamp, and the worker joined before any
`cudaHostUnregister`.

### 2.1 Throttle removed (new defaults, still env-overridable)

| knob | EXP-077 default | EXP-078 default | env override |
| --- | ---: | ---: | --- |
| batch cap | 4 MiB | 64 MiB | `GGML_EXPERT_PIN_BATCH_MB` |
| inter-batch sleep | 100 ms | 0 ms | `GGML_EXPERT_PIN_BATCH_SLEEP_MS` |

The queue bound `GGML_CUDA_EXPERT_PIN_QUEUE_MAX = 65536` is unchanged. The 0 ms sleep is safe
because the worker never busy-waits: with an empty queue it blocks on the condition variable, and
while the idle gate is not satisfied it blocks in `wait_for`. The only tight loop is the intended
continuous pinning during a quiet window, and it is bounded by the batch cap. The gate measures
this directly: a 2 s post-drain spin probe over an idle worker used 0.024 s of process CPU.

### 2.2 Idle gate unchanged (kept verbatim from EXP-077)

The worker starts a batch only after both:

1. the first-dispatch guard has fired (the first `set_tensor_async_pinned_ring` call returned);
2. `now - last_enqueue_ts >= GGML_EXPERT_PIN_IDLE_MS` (default 750 ms).

`ggml_cuda_expert_pin_enqueue_locked` still stamps `expert_pin_last_enqueue` under the pin mutex,
and `ggml_cuda_expert_pin_try_async` still skips `notify_one` while the worker is in the idle wait.

### 2.3 Bounded yield (new)

`enqueue_seq0` is the enqueue count (`expert_pin_n_queued`) at the moment the idle gate opens. The
batch loop compares it before every pop and after every `cudaHostRegister`; the first change means
the request stream resumed, so the batch closes at that range boundary:

```
if (cuda_ctx->expert_pin_n_queued != enqueue_seq0) { yielded = true; break; }
```

At most the one registration already in flight keeps competing with the starting request; when the
change is seen after a pin, that registration's bytes are recorded as the yield size. The next
outer iteration returns to the idle gate, which now holds a full idle window because
`last_enqueue` was just refreshed. So at most one in-flight batch can overlap a resume, and in
practice only the in-flight range does.

New counters (guarded by `g_expert_pin_mutex`): `expert_pin_n_yield`, `expert_pin_yield_bytes_max`,
`expert_pin_yield_bytes_total`. The release summary grew from 19 to 22 fields by appending
`<n> yield events, <max> yield bytes max, <total> yield bytes total`. The harness parses the longest
format first, so EXP-075 and EXP-076 prefixes still parse.

## 3. Build route

- `cp -a work/llama.cpp-exp077-pin-async2 work/llama.cpp-exp078-pin-idlefast`
- `cp -a build/exp077-pin-async2 build/exp078-pin-idlefast`
- Every text file under the copied build dir that contained `exp077-pin-async2` was rewritten to
  `exp078-pin-idlefast` (1167 files), then `cmake -S work/llama.cpp-exp078-pin-idlefast -B
  build/exp078-pin-idlefast` reconfigured cleanly in 0.3 s.
- Baseline build (unchanged EXP-077 source): 4 min 39 s. The path rewrite changed the source path in
  every compile command, so CMake/Ninja recompiled all 142 CUDA and 420 CXX objects rather than only
  relinking. After the edit the `ggml-cuda` rebuild took 3 min 10 s. Sequential builds only.

## 4. Model-free gate

Harness `test-exp078-pinsrc.cpp` extends the EXP-077 harness with two new modes (`rate`, `yield`) and
a 22-field release parser; the existing modes (`runtime`, `async`, `idle`, `selftest`, `shifted`,
`adjacent`) are unchanged. The copied unchanged EXP-077 source was built and gated FIRST: full-list
hash `5bbf98f3...` (both passes) and scheduler `39505119...` were reproduced before any edit
(`baseline-rev-it1-p{1,2}.txt`, `baseline-sched*.bin`).

### 4.1 (a) Full 17886-range list, async ON, defaults (64 MiB, 0 ms), two passes

`exp070_ranges.txt` sha256 `8b58328e...`, 17886 ranges, 23 237 033 984 bytes.

| pass | trace direct | trace ring | hash-list sha256 |
| --- | ---: | ---: | --- |
| pass 1 (ring + enqueue) | 0 | 17886 | `5bbf98f3...` |
| pass 2 (after drain) | 17886 | 0 | `5bbf98f3...` |

Release counters: direct 17886, fallback 17886, queued 17886, worker registered 17886, 343 worker
batches (EXP-077 default: 4593), 0 dropped, 0 dedup, worker joined 1, bytes released
23 311 929 344, bytes still pinned 0, VmSwap 0, idle deferrals 1. First batch 28 742.4 ms after the
first enqueue (i.e. after pass 1 ended plus the idle window). Wall 1 min 00.54 s versus 8 min 56 s
for the identical run on the copied EXP-077 source at 4 MiB / 100 ms.

### 4.2 (b) async OFF reproduces EXP-075

`GGML_EXPERT_PIN_ASYNC=0` full-list run (`runtime`): direct 17886, split 0, fallback 0, 17886
intervals, 17886 physical regions, 23 311 929 344 bytes released, worker joined 0, hash
`5bbf98f3...`. The synthetic EXP-075 parity modes also match exactly: selftest, shifted and adjacent
all PASS with `GGML_EXPERT_PIN_ASYNC=0`.

### 4.3 (b') async ON through the `runtime` mode (no drain wait)

Fallback 17886, worker registered 0, idle deferrals 1, worker joined 1, bytes still pinned 0, hash
`5bbf98f3...`. This is the idle-gate signature: the request stream runs for the whole mode, so the
worker never opens a batch. `GGML_EXPERT_PIN_SOURCE=0` (ring only) also gives `5bbf98f3...`.

### 4.4 (c) register-fail falls back end to end

`GGML_EXPERT_PIN_REGISTER_FAIL=1`, async ON, async mode with drain, `--no-direct-assert`. Fallback
35772 (2 passes), register failures 17886, worker registered 0, bytes released 0, dedup 17886 on the
second pass, worker joined 1, hash `5bbf98f3...`.

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

`async --iterations 3 --passes 2 --wait-drain --trace` with the shipped defaults (64 MiB, 0 ms):
21 PASS / 0 FAIL, no crash; all six hash lists equal `5bbf98f3...`; every iteration reports worker
registered 17886, worker joined 1, bytes still pinned 0, VmSwap 0. Wall 2 min 57 s.

A variant with `GGML_EXPERT_PIN_IDLE_MS=0` restores the EXP-076 concurrent start: 7 PASS / 0 FAIL,
worker registered 17886, worker joined 1, hash `5bbf98f3...`. With the gate disabled the worker keeps
up with the enqueue stream (one range per batch, queue often empty), so no bounded yield fires.

### 4.7 Idle-gate case

`idle --max-ranges 1000` with the shipped defaults. A continuous stream of 1000 ranges ran for
1845.8 ms with a maximum inter-enqueue gap of 27.5 ms. The worker issued NO batch while the stream
was active; the first batch opened at 2565.1 ms measured from the first enqueue (stream plus the
750 ms idle window), idle deferrals 1. After the stream went quiet the worker drained in 23 batches
(EXP-077 default: 299) and the second pass was fully direct; both hash lists are byte-identical
(`b5b2a9dc...`), VmSwap 0.

### 4.8 (f) Supplementary: budget saturation

`GGML_EXPERT_PIN_SOURCE_BUDGET_MB=64`: saturated 1, 51 ranges registered (64 786 432 bytes),
17 836 dropped, 1 worker batch, both hash lists `5bbf98f3...`, no spin. The drain wait is not
applicable under saturation, so that one harness check reports FAIL; the saturation counters and the
hash anchor are the evidence.

## 5. New gate measurements

### 5.1 Pin rate during a simulated quiet window

`rate --max-ranges 1000` enqueues 1000 ranges back to back, goes quiet, and waits for the worker to
drain the queue; the rate is the registered bytes divided by the quiet window. The window is measured
from the last enqueue; the gate delay is `first batch delay` minus the enqueue-stream duration.

| setting | ranges | bytes pinned | quiet window | gate wait | active pin | rate (window) | rate (active) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| shipped default 64 MiB / 0 ms | 1000 | 1 546 723 328 | 1054.5 ms | 723.0 ms | 331.4 ms | 1.467 GB/s | 4.667 GB/s |
| EXP-077 4 MiB / 100 ms | 1000 | 1 546 723 328 | 31 348.0 ms | 718.4 ms | 30 629.6 ms | 49.34 MB/s | 50.50 MB/s |

Both settings were measured with the same procedure and the same range list. The shipped default is
29.7x faster by quiet-window rate and 92.4x faster by active-pin rate. Both runs report worker joined
1, bytes still pinned 0, VmSwap 0. The default run also ran a 2000 ms post-drain spin probe: process
CPU grew by 0.024 s, so the 0 ms sleep does not spin.

### 5.2 Two-second quiet-window extrapolation (EXTRAPOLATION)

Model: a quiet window of 2.0 s between two requests; the worker waits the measured gate (about
0.72 s) and then pins at the measured active rate for the remaining about 1.28 s.

- shipped default: 4.667 GB/s x 1.277 s = 5.96 GB = 5.55 GiB pinned per 2 s window.
- to cover the about 21.7 GiB a 395-token prefill touches: 21.7 / 5.55 = 3.91, so about 4 such
  windows.
- conservative alternative using the window rate: 1.467 GB/s x 2 s = 2.93 GB = 2.73 GiB per window,
  so about 8 windows.
- EXP-077 setting for the same model: 50.50 MB/s x 1.282 s = 64.7 MB = 0.0603 GiB per window, so
  about 360 windows (about 236 by the conservative window-rate model). This is the arithmetic behind
  EXP-077 pinning only about 1.2 GiB in a session.

These numbers are extrapolations from a 1.05 s window to a 2 s window; they assume the active rate
holds for 1.28 s (four times the measured active phase). They are not a measurement of the model.

### 5.3 Yield: bytes pinned after a resume and ring ranges during the overlap

`yield --first-ranges 800 --resume-ranges 300`: the queue is filled during a stream, the worker
opens its first batch in the quiet window, then the stream resumes. Three runs:

| run | yield events | yield bytes max | ring ranges during overlap | ring ranges in phase B | pass 1 == pass 2 |
| --- | ---: | ---: | ---: | ---: | --- |
| 1 | 1 | 3 153 920 B (3.01 MiB) | 1 | 300 | yes |
| 2 | 1 | 1 576 960 B (1.50 MiB) | 1 | 300 | yes |
| 3 | 1 | 1 576 960 B (1.50 MiB) | 1 | 300 | yes |

Every run: worker closed exactly one batch on the resume, the bytes pinned after the resume fit in
one in-flight range (the largest range in the list is 11 981 312 B = 11.4 MiB), and exactly ONE range
was copied through the ring while the worker was still closing the yielded batch. All 1100 ranges
were registered after the resume stream went quiet, the post-drain pass was fully direct, joined 1,
bytes still pinned 0, VmSwap 0. The theoretical bound if a single range filled a batch is the 64 MiB
batch cap; the measured worst case is 3.0 MiB.

## 6. Build integrity: copied build dir RUNPATH

The copied build dir carries the old `DT_RUNPATH` inside every copied executable and shared library
(the EXP-076 defect that silently invalidated its first live A/B). Here the path rewrite forced a
full recompile and relink of every target, so the relink step found no stale target: the sweep
covers 110 files, 0 has a stale RUNPATH, and 92 resolve `libggml-cuda.so.0` from
`build/exp078-pin-idlefast/bin`. Full table in `runpath-sweep.txt`; the key rows:

| file | RUNPATH inside own bin | `ldd` libggml-cuda.so.0 |
| --- | --- | --- |
| `libggml-base.so.0.22.0` | n/a (no RUNPATH) | not linked |
| `libggml-cpu.so.0.22.0` | yes | not linked |
| `libggml-cuda.so.0.22.0` | yes | not linked (itself) |
| `libggml.so.0.22.0` | yes | `build/exp078-pin-idlefast/bin/libggml-cuda.so.0` |
| `libllama-common.so.0.3.0` | yes | `build/exp078-pin-idlefast/bin/libggml-cuda.so.0` |
| `libllama.so.0.3.0` | yes | `build/exp078-pin-idlefast/bin/libggml-cuda.so.0` |
| `libllama-server-impl.so` | yes | `build/exp078-pin-idlefast/bin/libggml-cuda.so.0` |
| `libmtmd.so.0.3.0` | yes | `build/exp078-pin-idlefast/bin/libggml-cuda.so.0` |
| `llama-server` | yes | `build/exp078-pin-idlefast/bin/libggml-cuda.so.0` |

Marker: `strings build/exp078-pin-idlefast/bin/libggml-cuda.so.0.22.0 | grep -c "async %s"` = 1, so the
async/idle/yield code is reachable. The reference builds are untouched:
`build/exp077-pin-async2/bin/libggml-cuda.so.0.22.0` `5edac915...`,
`build/exp076-pin-async/...` `8ecc3a71...`, `build/exp075-pinsrc-overlap/...` `27137a1a...`,
`build/exp074-pinsrc/...` `36dba1b2...`.

## 7. Artifacts

Build (`build/exp078-pin-idlefast/bin/`):

- `llama-server` sha256 `d399a97de0461f05ed2b7798753acd70ab213904638529c510d4f4d55460b7f2`
- `libggml-cuda.so.0.22.0` sha256 `22d15db096b412f05751296dd6a3ac7413d92f4fb9801e195796e6e622ebaae2`

Gate files in `results/archive/EXP-2026-09-17-078/`:

- `pin-idlefast.patch` sha256 `505d8907...` - exact diff EXP-077 tree -> EXP-078 tree
- `test-exp078-pinsrc.cpp` / `test-exp078-pinsrc` - gate harness with `rate` and `yield`
- `test-exp074-sched.cpp` / `test-exp074-sched` - scheduler invariant harness
- `baseline-rev-it1-p{1,2}.txt`, `baseline-sched*.bin` - pre-edit baseline proof
- `a-async-full.{out,log}`, `a-async-full-it1-p{1,2}.txt` - gate (a)
- `b-asyncoff.{log,txt}`, `b-asyncon.{log,txt}`, `ringonly.{log,txt}` - gates (b) and (b')
- `parity-{selftest,shifted,adjacent}-asyoff.log` - EXP-075 parity
- `c-regfail.{out,log}`, `c-regfail-it1-p{1,2}.txt` - gate (c)
- `sched-{asyncON,asyncOFF,splitoff,regfail}.{log,bin}` - gate (d)
- `e-stress.{out,log}`, `e-stress-it{1,2,3}-p{1,2}.txt` - gate (e)
- `e2-concurrency-idle0.{out,log}`, `e2-concurrency-idle0-it1-p{1,2}.txt` - IDLE_MS=0 variant
- `f-sat.{out,log}`, `f-sat-it1-p{1,2}.txt` - supplementary saturation
- `d-idle.{out,log}`, `d-idle-p{1,2}.txt` - idle-gate regression
- `rate-default.{out,log}`, `rate-exp077.{out,log}` - pin rate for both settings
- `yield-{1,2,3}.{out,log}`, `yield-1.txt` - bounded-yield case
- `runpath-sweep.txt`, `configure.log`, `build-baseline.log`, `build-idlefast.log` - build integrity
- `environment.txt` - host, toolchain, binary, harness, patch and ranges hashes

## 8. Not covered

- The live A/B is NOT run here. This session produces a build and a model-free gate only, and makes
  no throughput claim. The live re-measurement of the 12-prompt session against the accepted ring arm
  is the next step and needs Igor's approval.
- The rate was measured on the first 1000 ranges of the gate list, which are warm in the page cache;
  the full 23.3 GiB drain averaged a lower rate (gate (a) took 1 min 00.54 s for both passes). The
  two throttle settings were measured on the same list, so the comparison is fair, but the absolute
  rate is an upper bound.
- The 2 s window figure is an extrapolation from a 1.05 s measured window and is labelled as such.
- The gate drives the backend interface directly; it does not prove the server produces quiet windows
  long enough or frequent enough for the worker to cover a session. That is what the live A/B must
  confirm.

## 9. Open follow-up

Live A/B of `build/exp078-pin-idlefast` against the accepted ring arm, cache-balanced order, prompts
1 and 2 as the decision points and the session total as the verdict. If prompts 1 and 2 are neutral
while prompts 3+ keep the EXP-076 win, the fast idle-gated worker with the bounded yield becomes the
candidate default.

## Live A/B attempt on 2026-09-17 (INVALID)

A balanced 8-run session A/B of `PRE-PIN3c` was attempted on 2026-09-17 and is INVALID. The run used
`build/exp078-pin-idlefast`, one binary for both arms, the only variable env `GGML_EXPERT_PIN_SOURCE`
(A=0 ring only, B=1 idle-gated unthrottled worker), 12 prompts per run with a 4 second pause between
prompts, and the order A1 B1 B2 A2 A3 B3 B4 A4. Every single run had non-zero process swap, including
the ring-only arms that pin nothing, so the batch violates the project requirement that process swap
stays at zero and is discarded.

Max process swap per run:

| run | arm | max process swap (KiB) |
| --- | --- | ---: |
| A1 | ring only (A=0) | 456960 |
| A2 | ring only (A=0) | 1659648 |
| A3 | ring only (A=0) | 823040 |
| A4 | ring only (A=0) | 873984 |
| B1 | pin (B=1) | 752128 |
| B2 | pin (B=1) | 675584 |
| B3 | pin (B=1) | 2023680 |
| B4 | pin (B=1) | 805376 |

Max RSS across the batch was 39.6 to 45.4 GiB. One async run (B2) stopped answering in the middle:
requests 10 to 12 got no response and the process was gone before it could be interrupted.

Root cause is environmental, not the candidate. At the time the machine had 24.9 to 25.5 GiB of
system swap already in use, held by desktop applications (gnome-shell 5.35 GiB, Telegram 1.89 GiB,
several opencode sessions, ghostty, chrome), leaving about 44 GiB MemAvailable while one server run
needs about 45 GiB resident before any pinning. The kernel evicted server pages, which violates the
zero-swap requirement and contaminates every latency.

The batch is quarantined in `results/archive/EXP-2026-09-17-078-long/ab-invalid-swap/`, alongside an
`INVALID.md` that already records these facts.

RAM-bound conclusion. The binding constraint on the pinned-source direction is SYSTEM RAM, not the
algorithm. Pinning 24 GiB plus the about 45 GiB resident footprint of the server cannot guarantee
zero swap on a 60 GiB machine that is also running a desktop. The earlier gain measured for the
aggressive EXP-076 build (+15.0 percent median on prompts 3 to 12) was produced while the machine
was also drifting into swap, which is not an acceptable operating point.

Reduced-budget next step. The next measurement must use a reduced pin budget of 8 to 12 GiB after
memory is freed, and every run must be gated on process swap staying at zero. This is tracked as
`PRE-PIN3d` in `experiments/HYPOTHESES-2026-09-15-runtime-speed.md`. The mechanism itself is not in
question: it stays verified model-free in this report (sections 4 and 5), with the pin rate, the
drain time, the bounded yield and the identical gate hashes recorded above.
