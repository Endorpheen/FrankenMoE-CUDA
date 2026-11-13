# EXP-2026-09-16-074 - pinned-source direct-DMA prefill: implementation and model-free gate

Status: `MODEL-FREE DONE` - implementation in an isolated tree, build, and the full model-free
gate PASS. No model and no server were run, so no throughput claim is made.
Committed: no.
Design source: `/tmp/exp073-designs/PREFILL-PINNED-SOURCE.md`
Accepted path: `work/llama.cpp-exp062-ring3`, `build/exp062-ring3` (EXP-062 ring depth 3)
Isolated tree: `work/llama.cpp-exp074-pinsrc`
Build: `build/exp074-pinsrc`

## 1. Chosen option

Option A0 from the design: register the runtime's own `MAP_SHARED PROT_READ` GGUF mapping in
place and issue `cudaMemcpyAsync` directly from the registered source. No loader change and no
`MAP_PRIVATE` alias. The accepted primary mapping and the accepted pinned ring are untouched;
the direct copy is a new branch inside `ggml_backend_cuda_set_tensor_async_pinned_ring`.

G1 (below) was run first and decided A0 over Option A: the design's claim that `MAP_SHARED`
cannot be registered was never tested by EXP-070 (its probe hardcodes `MAP_PRIVATE`), and the
runtime flags register a `MAP_SHARED PROT_READ` mapping successfully.

## 2. G1 - mapping/registration probe

`g1_pinprobe.cpp` (runtime API, exact runtime flags); raw output in `g1_pinprobe.txt`.

| mapping | flags | result |
| --- | --- | --- |
| `MAP_SHARED PROT_READ` | `Portable|ReadOnly` | OK |
| `MAP_SHARED PROT_READ` | default | `invalid argument` |
| `MAP_PRIVATE PROT_READ` | `Portable|ReadOnly` | OK |
| `MAP_PRIVATE PROT_READ` | default | `invalid argument` |
| anonymous 16 MiB | `Portable|ReadOnly` | OK |
| anonymous 16 MiB | default | OK |

`RLIMIT_MEMLOCK` is 8192 KiB and there is no `CAP_IPC_LOCK`, yet driver registration succeeds:
the 8 MiB memlock is not enforced for `cudaHostRegister` on this host (U2 remains flagged, the
run-time must not depend on it; the fallback covers a future change).

## 3. Implementation

Two files, both in `ggml/src/ggml-cuda/` (exact diff in `pinsrc.patch`, 299 lines):

- `common.cuh`: context state for the interval registry, budget accounting, counters, declared
  after the fields mirrored by the EXP-041 model-free harness so its `streams[]` offset is
  unchanged.
- `ggml-cuda.cu`: the registry, the direct branch, a fault-injection hook, and teardown.

Behaviour:

- `GGML_EXPERT_PIN_SOURCE` default on; `0` disables (falls back to the accepted pinned ring).
- `GGML_EXPERT_PIN_SOURCE_BUDGET_MB` default 24576 (24 GiB); hard process-wide cap.
- `GGML_EXPERT_PIN_REGISTER_FAIL` fault injection, forces the fallback end to end.
- Page-align the source range, register it once read-only, then one `cudaMemcpyAsync` of exactly
  the requested size on the same compute stream. The range passed by the scheduler already
  includes the <= 512 B expert padding (`ggml-backend.cpp:1702-1703`), so the registered interval
  covers the whole copy (design 3.5).
- Registration is refused when it would overlap an existing registration, exceed the budget, or
  drop `MemAvailable` below the launcher's 8 GiB margin; each refusal falls back to the ring.
  A failed registration is blacklisted so it is never retried.
- Teardown: device synchronize, then `cudaHostUnregister` for every interval, in the backend
  context destructor while the CUDA context is current, before the loader unmaps the mapping.

## 4. Model-free gate over the real 17886 ranges

Harness `test-exp074-pinsrc.cpp` drives `set_tensor_async_pinned_ring` with the real EXP-045/070
range list, reads the device bytes back per range and hashes them (FNV-1a 64, EXP-070 format),
then sha256s the hash list. The accepted reference is `build/exp062-ring3` run with the same
harness. All runs are sequential, one process at a time.

| run | build | env | direct / fallback / fail | ring calls | hash-list sha256 |
| --- | --- | --- | --- | ---: | --- |
| pin source on | exp074 | default | 17886 / 0 / 0 | 0 | `5bbf98f3...` |
| pin source off | exp074 | `GGML_EXPERT_PIN_SOURCE=0` | 0 / 0 / 0 | 17886 | `5bbf98f3...` |
| register fail | exp074 | `GGML_EXPERT_PIN_REGISTER_FAIL=1` | 0 / 17886 / 17886 | 17886 | `5bbf98f3...` |
| budget 1 MiB | exp074 | `GGML_EXPERT_PIN_SOURCE_BUDGET_MB=1` | 1 / 17885 / 0 | 17885 | `5bbf98f3...` |
| accepted | exp062 | none | n/a | 17886 | `5bbf98f3...` |

`5bbf98f3f18049105f5fe7e758a6bda033c67d2fc1accf3bf5b7b528e90dcde8` is the EXP-045/070 verified
hash-list sha256 (`results/archive/EXP-2026-09-16-070/REPORT.md:72-74`). It is reproduced exactly
by the direct path and by every fallback mode.

- Ranges: 17886, bytes 23 237 033 984 (21.6412 GiB), 0 refused in every run.
- Registered (page-aligned) bytes for the direct run: 23 311 929 344 (21.7109 GiB), 17886
  intervals, 0 failures, over 23 211 929 344 of raw ranges; overhead 74 895 360 B (71.4 MiB),
  identical to EXP-070.
- Wall for one full pass: direct 28.52 s, ring 26.29 s, register-fail 26.42 s, budget 26.59 s,
  accepted 26.63 s (direct includes the one-time cold page-in plus pin of 21.71 GiB).
- Peak RSS for the direct run: 24 201 276 KiB (~23.08 GiB). Swap: 0.

## 5. G5 - scheduler invariant

`test-exp074-sched.cpp` runs the accepted 395-token MUL_MAT_ID prefill shape through
`ggml_backend_sched`, so the real `ggml-backend.cpp` weights branch calls the ring interface.

| run | direct / fallback / fail | output sha256 |
| --- | --- | --- |
| pin source on | 1 / 0 / 0 | `39505119b144ea286e865119b397eecff75578881c24c4aa1cec9bd642e91510` |
| `GGML_EXPERT_PIN_SOURCE=0` | 0 / 0 / 0 | `39505119...` |
| `GGML_EXPERT_PIN_REGISTER_FAIL=1` | 0 / 1 / 1 | `39505119...` |
| accepted exp062 | n/a | `39505119...` |

`39505119...` is the accepted EXP-038/041/062 scheduler output hash, so the direct path does not
change the scheduler output.

## 6. G6 - teardown and synthetic correctness (`selftest`)

- Covered ranges copied byte-exact; sentinel guards unchanged; source mapping unchanged.
- Modes exercised: page-disjoint (direct), page-adjacent (direct), a range contained in a
  registered range (direct, free), a page-aligned-end range (direct), and a range that overlaps a
  registered range (fallback, byte-exact).
- Counters: 6 direct, 1 fallback, 0 register failures, 4 intervals released.
- `VmPin` 0 before, during, and after free; `VmSwap` 0. All registrations are released before the
  backend is freed.

## 7. Safety notes

- Bounded pinned RAM: hard 24 GiB process-wide budget, plus the 8 GiB `MemAvailable` margin. Both
  are enforced per registration; a refusal is a fallback, never a partial or unsafe copy.
- The fallback is logged once per context; the reason (registration failed, budget, overlap, low
  headroom) is named.
- Selection change: an already-registered range is copied directly; a range that would overlap an
  existing registration falls back without unregistering or splitting. The real 17886-range list
  has zero page-level overlaps, so a full prefill stays entirely on the direct path. A later
  prompt that selects a partially overlapping set degrades to the gather path for those ranges.
- `VmSwap` stayed 0 in every run; `pin_before`/`pin_after`/`pin_after_free` were 0 in every run
  (`VmPin` counts `mlock`, not driver pins).

## 8. Artifacts

Build (`build/exp074-pinsrc/bin/`):

- `llama-server` sha256 `9da1c3ef5840234a9d2252d84ba4c62f13e0a0cb3b6570b65ae677d0ff2a8093`
- `libggml-cuda.so.0.22.0` sha256 `36dba1b2c98bacb96e752e0c72fd5ce43e504a5b24585c88c9456494b772403f`

Reference (`build/exp062-ring3/bin/`):

- `llama-server` sha256 `f4b13f6b6f5cfef2529a42f9c32e5d7c6937d1baf217a66bfe1b5af2e5f7a1b1`
- `libggml-cuda.so.0.22.0` sha256 `7738505e70495b9893426539ec611e61d2ac686c30ab3c39a2e45e1b7859577c`

Gate files in `results/archive/EXP-2026-09-16-074/`:

- `g1_pinprobe.cpp`, `g1_pinprobe.txt` - G1 probe and raw output
- `test-exp074-pinsrc.cpp`, `test-exp074-pinsrc` - real-range and selftest harness
- `test-exp074-sched.cpp`, `test-exp074-sched` - scheduler-invariant harness
- `selftest-exp074.txt` - G6 output
- `rev-hash-pinsrc-on.txt`, `rev-hash-pinsrc-off.txt`, `rev-hash-regfail.txt`,
  `rev-hash-budget1.txt`, `rev-hash-accepted.txt` - full-list hash lists
- `run-pinsrc-on.log`, `run-pinsrc-off.log`, `run-regfail.log`, `run-budget1.log`,
  `run-accepted.log` - raw run logs
- `sched-on.bin`, `sched-off.bin`, `sched-fail.bin`, `sched-accepted.bin` - scheduler outputs
- `pinsrc.patch` - exact diff against `work/llama.cpp-exp062-ring3`

Changed in the isolated tree only:

- `work/llama.cpp-exp074-pinsrc/ggml/src/ggml-cuda/common.cuh`
- `work/llama.cpp-exp074-pinsrc/ggml/src/ggml-cuda/ggml-cuda.cu`

No accepted tree, accepted build, or `work/llama.cpp-integration` was modified, and nothing was
committed.

## 9. Not covered

- No model and no server were run; no prefill or decode claim is made.
- The design's model A/B (Arm A = `build/exp063-default-runtime`, Arm B = this build, 5 pairs,
  byte-identical content, `VmSwap = 0`) remains the next step and needs Igor's approval.
- The first-prefill cold cost of pinning 21.71 GiB measured here is 28.52 s for one full range
  pass, dominated by the cold page-in plus pin; the live server must report first-request latency.
- Multi-context budget: the byte cap is process-wide, but the interval registry is per backend
  context; a target plus draft context would register their own sources under the shared cap.
