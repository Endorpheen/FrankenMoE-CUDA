# EXP-2026-09-16-075 - pinned-source overlap split: implementation and model-free gate

Status: `MODEL-FREE DONE - BUGFIX` - implementation in an isolated tree, build, the full model-free
gate PASS, and a physical-boundary bug found by the live A/B fixed and covered by a new gate case.
No model and no server were run for the fix, so no throughput claim is made.
Committed: no.
Hypothesis: `PRE-PIN2` in `experiments/HYPOTHESES-2026-09-15-runtime-speed.md`.
Base: `results/archive/EXP-2026-09-16-074/REPORT.md` (EXP-074 pinned-source direct-DMA).
Isolated tree: `work/llama.cpp-exp075-pinsrc-overlap`
Build: `build/exp075-pinsrc-overlap`

## BUGFIX (2026-09-16): split path crossed a physical registration boundary

### Live abort

A live A/B on binary `build/exp075-pinsrc-overlap` with `GGML_EXPERT_PIN_SOURCE=1` (split default
ON), port 8087, fresh server, request sequence warmup, `p1` (395-token prompt), `p2` (a different
396-token prompt with a partly overlapping expert set), `p1b` (p1 again): `p1` returned 200, `p2`
aborted the process, `p1b` never ran. Deterministic in all five runs. Ring-only and
`GGML_EXPERT_PIN_OVERLAP_SPLIT=0` runs completed every request. Raw logs `results/e075-B1.log` ..
`results/e075-B5.log`.

```
CUDA error: invalid argument
  current device: 0, in function ggml_cuda_expert_pin_try_split at work/llama.cpp-exp075-pinsrc-overlap/ggml/src/ggml-cuda/ggml-cuda.cu:2856
  cudaMemcpyAsync(dst_bytes + off, seg.host, seg.bytes, cudaMemcpyHostToDevice, cuda_ctx->stream())
```

### Root cause

`ggml_cuda_expert_pin_insert` merges TOUCHING logical intervals (`it->first <= end`) and
`ggml_cuda_expert_pin_cover_end` returned the end of the merged logical interval. The merge is only
logical: the bytes came from separate `cudaHostRegister` calls, and CUDA rejects a copy that starts
inside one physical registration and runs past its end. EXP-074 `ggml_cuda_expert_pin_try_whole` did
not hit this on the model because it copied the page-aligned range it had registered in one
physical call. `ggml_cuda_expert_pin_try_split` took the covered run out to the merged logical end,
so on the second, partly overlapping prompt any covered run spanning a merged boundary aborted.
`p2` always has such runs because `p1` registered many page-adjacent expert ranges that merged. The
old gate missed it because the shifted scenario registered set A with 1 MiB gaps, so no two
registrations were page-adjacent and no covered run could span a merge boundary.

### Fix

Copy boundary is now physical, not logical:

- `common.cuh`: new `std::vector<std::pair<uintptr_t, uintptr_t>> expert_pin_regions`, one entry per
  successful `cudaHostRegister`, kept sorted by start and never merged or split. It is the ground
  truth for what CUDA accepts as one copy source.
- `ggml-cuda.cu`: `ggml_cuda_expert_pin_insert_region` records the region after
  `ggml_cuda_expert_pin_register` returns true. `ggml_cuda_expert_pin_try_split` now takes coverage
  and gap boundaries from `expert_pin_regions`, so a covered sub-run is clamped to the end of the
  single physical region that contains `cur`; the walk then continues and either enters another
  physical region or registers a remainder. Segment bookkeeping is unchanged, so the concatenated
  sub-copies still cover `[src, src+size)` once, in order, with no gap and no overlap.
- `expert_pin_intervals` is unchanged: it still drives budget, overlap, blacklist and merge logic.
- Teardown does one `cudaHostUnregister` per physical region (previously one per merged interval,
  which would have left a touching registration pinned) and the release log reports
  `physical regions released`.
- `GGML_EXPERT_PIN_OVERLAP_SPLIT=0` still selects `ggml_cuda_expert_pin_try_whole` verbatim, and
  `GGML_EXPERT_PIN_REGISTER_FAIL` still forces the fallback end to end.

The physical clamp is enforced at
`work/llama.cpp-exp075-pinsrc-overlap/ggml/src/ggml-cuda/ggml-cuda.cu:2824-2830`: `cover` comes from
`expert_pin_regions` and the run end is `std::min<uintptr_t>(cover, req_end)`, with the comment that
a `cudaMemcpyAsync` from registered memory must not cross a physical registration end.

### New gate case: adjacent-merge (must fail on the old code)

Harness mode `adjacent` registers two PAGE-ADJACENT ranges as two separate requests
(`[P, P+4 MiB)` and `[P+4 MiB, P+8 MiB)`) and then requests `[P+1 MiB, P+7 MiB)`, which spans the
shared boundary hidden by the logical merge. The device image is checked byte-exact against the
source with sentinel guards on both sides.

| run | direct / split / fallback | intervals | physical regions | covered / registered sub-copies | bytes released | bytes still | swap | checks |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| fixed, split ON | 3 / 0 / 0 | 1 | 2 | 2 / 2 | 8388608 | 0 | 0 | 6 PASS / 0 FAIL |
| pre-fix, split ON | - | - | - | - | - | - | - | SIGABRT `invalid argument` |

The pre-fix negative control aborts at `ggml_cuda_expert_pin_try_split:2856` with
`cudaMemcpyAsync` and `invalid argument`, exactly the live failure; capture `prefix-adjacent-abort.log`.
The same extended scenario run as `shifted` also aborts pre-fix at the B spanning request
(`prefix-shifted-abort.log`). Negative controls were captured on the pre-fix binary linked against
the pre-fix `libggml-cuda.so.0.22.0` (sha256 `214ac010...`), before the rebuild.

### Extended shifted scenario

Set A now also holds two page-adjacent 4 MiB registrations, and one set-B request spans their shared
boundary; the earlier A/B requests are unchanged.

| run | direct | split | fallback | intervals | physical regions | sub-direct | sub-reg | bytes released | bytes still | swap | VmHWM KiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| fixed, split ON | 18 | 8 | 0 | 2 | 18 | 24 | 18 | 41943040 | 0 | 0 | 618020 |
| fixed, split OFF | 18 | 0 | 8 | 9 | 10 | 0 | 0 | 33554432 | 0 | 0 | 618544 |
| fixed, register fail | 0 | 0 | 26 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 618284 |
| pre-fix, split ON | - | - | - | - | - | - | - | - | - | - | SIGABRT |

### Full-list, scheduler and teardown after the fix

| run | env | direct / split / fallback | register failures | intervals | physical regions | bytes released | peak RSS KiB | swap | hash-list sha256 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| split ON | default | 17886 / 0 / 0 | 0 | 17886 | 17886 | 23311929344 | 24184172 | 0 | `5bbf98f3...` |
| split OFF | `..._OVERLAP_SPLIT=0` | 17886 / 0 / 0 | 0 | 17886 | 17886 | 23311929344 | 24185400 | 0 | `5bbf98f3...` |
| register fail | `..._REGISTER_FAIL=1` | 0 / 0 / 17886 | 17886 | 0 | 0 | 0 | 24113264 | 0 | `5bbf98f3...` |
| ring only | `..._PIN_SOURCE=0` | n/a (ring) | n/a | 0 | 0 | 0 | 24111416 | 0 | `5bbf98f3...` |

`5bbf98f3f18049105f5fe7e758a6bda033c67d2fc1accf3bf5b7b528e90dcde8` is still reproduced by every
mode. The 17886-range list has no page-level overlap, so the split path stays on the
direct/registered path and intervals equal physical regions (17886). `bytes still pinned` was 0 in
every run and `VmSwap` stayed 0.

| scheduler run | output sha256 |
| --- | --- |
| split ON | `39505119b144ea286e865119b397eecff75578881c24c4aa1cec9bd642e91510` |
| `GGML_EXPERT_PIN_SOURCE=0` | `39505119...` |
| `..._REGISTER_FAIL=1` | `39505119...` |
| `..._OVERLAP_SPLIT=0` | `39505119...` |

Teardown: `bytes still pinned` 0 and `physical regions released` equal to the number of
`cudaHostRegister` calls in every run (adjacent 2, shifted ON 18, full list 17886); `VmSwap` 0.

### Bugfix artifacts

- `pinsrc-overlap.patch` sha256 `0874e83dfed561e1180c0269578307105b466f46eb8d863af2f856c45d3a702c`
- fixed `libggml-cuda.so.0.22.0` sha256 `27137a1a3ea0308c7712a3c15b944438e8c5ff2741d681f2ec685bd331ecd45a`
- `llama-server` sha256 `4aec1132ec4febae68e050aa93a58397dcb09feec4e640f130d8cedcb32bdafb` (unchanged:
  the executable links the CUDA library dynamically, the fix is in `libggml-cuda`)
- `test-exp075-pinsrc` sha256 `74d3e375236b066fbc76be1ccc99ac0cebf209eab32e03c9053ba7b5f8d6672d`
- `test-exp074-sched` sha256 `5747eab50014eddfaa5c3326b479d2ad875e6bf8fc2d5cd6a4e6e9b000bb10e1`
- gate outputs `bugfix-*`, negative controls `prefix-{adjacent,shifted}-abort.log`,
  build log `build-bugfix.log`

## 1. Goal

EXP-074 served a request directly only when the whole page-aligned range was already registered.
Any partial overlap with an existing registration was refused and the request fell back to the
pinned slot ring. The EXP-074 caveat probe showed the cost: a repeated 395-token prompt was
+15.17 percent with the direct path, but a novel 396-token prompt was -5.27 percent because its
expert set only partly overlapped the earlier registrations
(`experiments/EXP-2026-09-16-070-prefill-pinned-source-direct-dma.md`, section
`### Caveat probe (2026-09-16)`).

This experiment serves a partly overlapping request from the registered pages by splitting it at
the registration boundaries. The direct path now covers novel prompts that overlap the registered
set instead of falling back.

## 2. Mechanism

Changed in the isolated tree only (exact diff in `pinsrc-overlap.patch`, 208 lines, 5 hunks):

- `work/llama.cpp-exp075-pinsrc-overlap/ggml/src/ggml-cuda/common.cuh`
- `work/llama.cpp-exp075-pinsrc-overlap/ggml/src/ggml-cuda/ggml-cuda.cu`

New env `GGML_EXPERT_PIN_OVERLAP_SPLIT`, default ON in this build. `0` restores the EXP-074 rule
(any overlap refuses the whole request and falls back to the ring), so the two behaviours can be
A/B'd with one binary. `GGML_EXPERT_PIN_SOURCE`, `GGML_EXPERT_PIN_SOURCE_BUDGET_MB` (24576 MiB)
and `GGML_EXPERT_PIN_REGISTER_FAIL` are unchanged.

`ggml_cuda_expert_pin_try_split` walks the request `[src, src+size)` and cuts it at registration
boundaries:

- a sub-run already covered by one registered interval is one `cudaMemcpyAsync` from the registered
  host address, no new registration (`direct` sub-copy);
- an uncovered sub-run is page-aligned only for the registration, registered under the existing
  budget and 8 GiB `MemAvailable` margin rules, then copied (`registered` sub-copy).

The plan registers every uncovered remainder before the first copy is issued, so a refusal falls
back with nothing copied - no double copy, no partial image. Sub-copy sizes follow the original
unaligned request boundaries (the scheduler range already includes the <= 512 B expert padding,
`ggml-backend.cpp:1702-1703`), so the copies cover `[src, src+size)` exactly: no gap, no byte
outside the request, and the total copied byte count equals the request size. Coverage and gap
boundaries come from `expert_pin_regions` (one entry per `cudaHostRegister`, never merged), not from
the merged `expert_pin_intervals`, so a covered run is clamped to the end of the one physical
registration that contains it and the `cudaMemcpyAsync` range never crosses a registration end (see
BUGFIX above; the pre-fix build took the run to the merged logical end and aborted). Copies are
issued in request order on the same compute stream.

Classification: `direct` = request fully covered or fully registered with nothing covered;
`split` = request had at least one covered sub-copy and at least one newly registered remainder;
`fallback` = any sub-range refused (register failure, blacklist, budget, low headroom, overlap in
EXP-074 mode). The release log now reports `direct`, `split`, `fallback`, register failures,
intervals released, physical regions released, bytes released, bytes still pinned, covered
sub-copies and registered sub-copies. Teardown is unchanged apart from the unit: the context
destructor does `cudaDeviceSynchronize`, then one `cudaHostUnregister` per physical region, before
the loader unmaps the mapping.

## 3. Build route

The task's cheap route was taken, not a clean full build:

- `cp -a work/llama.cpp-exp074-pinsrc work/llama.cpp-exp075-pinsrc-overlap`
- `cp -a build/exp074-pinsrc build/exp075-pinsrc-overlap`
- the copied `CMakeCache.txt` still named the EXP-074 source and binary dirs, so CMake refused to
  reconfigure; the cached absolute paths in `CMakeCache.txt` were rewritten to the EXP-075 paths
  and `cmake -S work/llama.cpp-exp075-pinsrc-overlap -B build/exp075-pinsrc-overlap` then
  configured cleanly (0.3 s).
- Baseline (unmodified tree) build: 449 objects, 3 min 29 s wall, exit 0.
- Build after the edit: 2 min 30 s wall, exit 0, no compile errors.

The accepted trees and builds (`work/llama.cpp-integration`, `work/llama.cpp-exp062-ring3`,
`work/llama.cpp-exp074-pinsrc`, `build/exp063-default-runtime`, `build/exp074-pinsrc`) were not
modified.

## 4. Model-free gate

Harness `test-exp075-pinsrc.cpp` (sha256 in `environment.txt`) drives
`set_tensor_async_pinned_ring` directly and captures the release counters through the `ggml_log`
callback. All runs are sequential, one process at a time.

### 4.1 Real 17886-range full list (hash `5bbf98f3...`)

`exp070_ranges.txt` sha256 `8b58328ed61b0bfea88021e7f0a895233a9a56f078fc65a9ababe4da6b3f6f79`,
17886 ranges, 23 237 033 984 bytes (21.6412 GiB).

| run | env | direct / split / fallback | register failures | intervals | bytes released | peak RSS KiB | swap | hash-list sha256 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| accepted exp062 | none | n/a (ring) | n/a | n/a | n/a | 24110700 | 0 | `5bbf98f3...` |
| split ON | default | 17886 / 0 / 0 | 0 | 17886 | 23311929344 | 24183916 | 0 | `5bbf98f3...` |
| split OFF | `..._OVERLAP_SPLIT=0` | 17886 / 0 / 0 | 0 | 17886 | 23311929344 | 24184180 | 0 | `5bbf98f3...` |
| register fail | `..._REGISTER_FAIL=1` | 0 / 0 / 17886 | 17886 | 0 | 0 | 24111416 | 0 | `5bbf98f3...` |

`5bbf98f3f18049105f5fe7e758a6bda033c67d2fc1accf3bf5b7b528e90dcde8` is the accepted EXP-045/070
verified hash-list sha256. It is reproduced exactly by the new build (split ON and split OFF), by
the register-fail fallback, and by the accepted `build/exp062-ring3` with the same harness. The
direct runs register 23 311 929 344 bytes (page-aligned), identical to EXP-074. The 17886-range
list has no page-level overlap, so both split modes stay entirely on the direct/registered path;
`bytes still pinned` was 0 in every run.

### 4.2 Scheduler invariant (G5)

`test-exp074-sched.cpp` runs the accepted 395-token MUL_MAT_ID prefill shape through
`ggml_backend_sched`, so the real `ggml-backend.cpp` weights branch calls the ring interface.

| run | output sha256 |
| --- | --- |
| split ON | `39505119b144ea286e865119b397eecff75578881c24c4aa1cec9bd642e91510` |
| `GGML_EXPERT_PIN_SOURCE=0` | `39505119...` |
| `..._REGISTER_FAIL=1` | `39505119...` |
| `..._OVERLAP_SPLIT=0` | `39505119...` |

`39505119...` is the accepted EXP-038/041/062 scheduler output hash, so the split path does not
change the scheduler output.

### 4.3 Shifted set B (register A, then request partly overlapping B)

The scenario registers set A = eight 3 MiB experts with 1 MiB gaps, then requests set B: eight
ranges fully inside an expert and seven "shifted" 3 MiB ranges that start inside one expert, cross
its end into the gap, and end inside the next expert. Byte-exactness is checked against a
filled device image with sentinel guards on both sides.

| run | direct | split | fallback | cover sub-copies | reg sub-copies | intervals | bytes released | bytes still | swap | VmHWM KiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| split ON | 16 | 7 | 0 | 22 | 15 | 1 | 32505856 | 0 | 0 | 511796 |
| split OFF | 16 | 0 | 7 | 0 | 0 | 8 | 25165824 | 0 | 0 | 512048 |
| register fail | 0 | 0 | 23 | 0 | 0 | 0 | 0 | 0 | 0 | 512048 |

- Byte-exact in every mode: `CHECK PASS shifted: device image byte-exact over A and B`, sentinel
  guards unchanged.
- Split ON: no fallback; the seven shifted requests are served by split sub-copies and the
  majority of sub-copies (22 of 37) come from already-registered pages, so the direct path extends
  to the novel set. Matches the requirement.
- Split OFF: the seven shifted requests are refused and fall back to the ring, and the output is
  byte-identical. This is the EXP-074 behaviour, and it is the negative control for the split.
- Register fail: all 23 requests fall back end to end; eight register attempts fail and blacklist
  their intervals, later requests overlap the blacklist and fall back without a new attempt; the
  output is byte-exact.

### 4.4 Synthetic selftest and teardown

| run | direct / split / fallback | intervals | bytes released | bytes still | checks |
| --- | --- | ---: | ---: | ---: | --- |
| split ON | 5 / 2 / 0 | 3 | 25169920 | 0 | 8 PASS / 0 FAIL |
| split OFF | 6 / 0 / 1 | 4 | 20979712 | 0 | 8 PASS / 0 FAIL |

Cases cover page-disjoint, page-adjacent, contained, partial-overlap (split ON) / refused (split
OFF) and page-aligned-end ranges; the device image is byte-exact, the sentinel guards and the
source mapping are unchanged, `VmPin` returns to its start value and `VmSwap` stays 0.

Teardown: `bytes still pinned` was 0 after every backend free, i.e. the context destructor
released every interval registered by the split path. `VmSwap` stayed 0 in every run.

## 5. Safety notes

- The split path reuses the EXP-074 budget (24 GiB process-wide cap) and the 8 GiB `MemAvailable`
  margin per registration; a refusal falls back, never a partial or unsafe copy.
- The plan registers all remainders before any copy, so a fallback does not double-copy bytes.
- Registration still refuses overlap; the split logic only passes non-overlapping page-aligned
  remainders to it.
- Bounded: the number of sub-copies per request is bounded by the number of registered intervals
  the request intersects.

## 6. Artifacts

Build (`build/exp075-pinsrc-overlap/bin/`), fixed build:

- `llama-server` sha256 `4aec1132ec4febae68e050aa93a58397dcb09feec4e640f130d8cedcb32bdafb` (unchanged)
- `libggml-cuda.so.0.22.0` sha256 `27137a1a3ea0308c7712a3c15b944438e8c5ff2741d681f2ec685bd331ecd45a`
  (pre-fix was `214ac0109c886559d81576d7d1038b7063fb14d56b38a79b6da27c26082e0305`, used only for
  the negative controls)

Gate files in `results/archive/EXP-2026-09-16-075/`:

- `pinsrc-overlap.patch` - exact diff `work/llama.cpp-exp074-pinsrc` -> `work/llama.cpp-exp075-pinsrc-overlap`
  including the bugfix, sha256 `0874e83dfed561e1180c0269578307105b466f46eb8d863af2f856c45d3a702c`
- `test-exp075-pinsrc.cpp`, `test-exp075-pinsrc` - runtime/selftest/shifted/adjacent harness,
  binary sha256 `74d3e375236b066fbc76be1ccc99ac0cebf209eab32e03c9053ba7b5f8d6672d`
- `test-exp074-pinsrc.cpp`, `test-exp074-pinsrc` - EXP-074 baseline harness used to prove the
  environment before the edit
- `test-exp074-sched.cpp`, `test-exp074-sched` - scheduler-invariant harness,
  binary sha256 `5747eab50014eddfaa5c3326b479d2ad875e6bf8fc2d5cd6a4e6e9b000bb10e1`
- `rev-hash-{accepted,split-on,split-off,regfail}.txt` - pre-fix full-list hash lists
- `run-{accepted,split-on,split-off,regfail}.{log,time}` - pre-fix raw run output and `/usr/bin/time -v`
- `selftest-{on,off}.txt`, `shifted-{on,off,regfail}.txt` - pre-fix synthetic and shifted scenarios
- `sched-{on,off,fail,splitoff}.{log,bin}` - pre-fix scheduler outputs
- `bugfix-adjacent-on.txt`, `bugfix-shifted-{on,off,regfail}.txt`, `bugfix-selftest-{on,off}.txt` -
  fixed-build synthetic gates
- `bugfix-run-{on,off,regfail,ringonly}.{log,time}`, `bugfix-rev-hash-*.txt` - fixed-build full list
- `bugfix-sched-{on,off,fail,splitoff}.{log,bin}` - fixed-build scheduler outputs
- `prefix-adjacent-abort.log`, `prefix-shifted-abort.log` - pre-fix negative controls
- `build-baseline.log`, `build-pinsplit.log`, `build-bugfix.log` - build logs
- `environment.txt` - host, toolchain, pre-fix and bugfix hashes

## 7. Not covered

- No model and no server were run in this fix session; no prefill or decode claim is made. The live
  A/B that found the abort (B1-B5) is the only runtime evidence, and it aborted. A fresh live A/B
  of the fixed library against the accepted runtime is needed before any runtime claim, with Igor's
  approval, and is a separate step.
- No throughput number is produced here. The expected benefit is bounded by the EXP-074 caveat
  probe: the split path should remove the `range overlaps a registered range` fallback for novel
  prompts, but that must be measured with the model.
- The split adds one `cudaMemcpyAsync` per covered/registered run instead of one copy per request;
  the transfer-time effect is not measured here.
