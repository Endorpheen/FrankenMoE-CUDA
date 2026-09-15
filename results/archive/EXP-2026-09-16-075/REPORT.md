# EXP-2026-09-16-075 - pinned-source overlap split: implementation and model-free gate

Status: `MODEL-FREE DONE` - implementation in an isolated tree, build, and the full model-free
gate PASS. No model and no server were run, so no throughput claim is made.
Committed: no.
Hypothesis: `PRE-PIN2` in `experiments/HYPOTHESES-2026-09-15-runtime-speed.md`.
Base: `results/archive/EXP-2026-09-16-074/REPORT.md` (EXP-074 pinned-source direct-DMA).
Isolated tree: `work/llama.cpp-exp075-pinsrc-overlap`
Build: `build/exp075-pinsrc-overlap`

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
outside the request, and the total copied byte count equals the request size. A registered
interval is merged, so a contiguous covered run is inside one registration and the
`cudaMemcpyAsync` range never crosses a registration end. Copies are issued in request order on
the same compute stream.

Classification: `direct` = request fully covered or fully registered with nothing covered;
`split` = request had at least one covered sub-copy and at least one newly registered remainder;
`fallback` = any sub-range refused (register failure, blacklist, budget, low headroom, overlap in
EXP-074 mode). The release log now reports `direct`, `split`, `fallback`, register failures,
intervals released, bytes released, bytes still pinned, covered sub-copies and registered
sub-copies. Teardown is unchanged: the context destructor does `cudaDeviceSynchronize`, then
`cudaHostUnregister` for every interval, before the loader unmaps the mapping.

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

Build (`build/exp075-pinsrc-overlap/bin/`):

- `llama-server` sha256 `4aec1132ec4febae68e050aa93a58397dcb09feec4e640f130d8cedcb32bdafb`
- `libggml-cuda.so.0.22.0` sha256 `214ac0109c886559d81576d7d1038b7063fb14d56b38a79b6da27c26082e0305`

Gate files in `results/archive/EXP-2026-09-16-075/`:

- `pinsrc-overlap.patch` - exact diff `work/llama.cpp-exp074-pinsrc` -> `work/llama.cpp-exp075-pinsrc-overlap`,
  sha256 `b6139b3544be756119c27e7100621a149f62e689a999faa5e09acf6d0fd8dc54`
- `test-exp075-pinsrc.cpp`, `test-exp075-pinsrc` - runtime/selftest/shifted harness,
  binary sha256 `4f1911a829db0418230eb221454069e51239cae205f89e4098bfa71e25b511ef`
- `test-exp074-pinsrc.cpp`, `test-exp074-pinsrc` - EXP-074 baseline harness used to prove the
  environment before the edit
- `test-exp074-sched.cpp`, `test-exp074-sched` - scheduler-invariant harness
- `rev-hash-{accepted,split-on,split-off,regfail}.txt` - full-list hash lists
- `run-{accepted,split-on,split-off,regfail}.{log,time}` - raw run output and `/usr/bin/time -v`
- `selftest-{on,off}.txt`, `shifted-{on,off,regfail}.txt` - synthetic and shifted scenarios
- `sched-{on,off,fail,splitoff}.{log,bin}` - scheduler outputs
- `build-baseline.log`, `build-pinsplit.log` - build logs
- `environment.txt` - host, toolchain, hashes

## 7. Not covered

- No model and no server were run; no prefill or decode claim is made. The live A/B of this
  candidate against the accepted runtime still needs Igor's approval and is a separate step.
- No throughput number is produced here. The expected benefit is bounded by the EXP-074 caveat
  probe: the split path should remove the `range overlaps a registered range` fallback for novel
  prompts, but that must be measured with the model.
- The split adds one `cudaMemcpyAsync` per covered/registered run instead of one copy per request;
  the transfer-time effect is not measured here.
