# EXP-2026-09-16-075: pinned-source overlap split, serve a partly overlapping request from the registered pages

Status: `MODEL-FREE DONE - BUGFIX`; live A/B done on the fixed build (2026-09-16).
Kind: `prototype-offline` model-free gate plus a live candidate A/B.

This card is the written verdict for the candidate tracked as `PRE-PIN2` in
`experiments/HYPOTHESES-2026-09-15-runtime-speed.md`. The model-free gate starts no model
and no `llama-server` and makes no throughput claim; the live A/B section below runs a
candidate `llama-server` binary but only reports prefill latency, not aggregate throughput.
It is not an accepted runtime change: the candidate lives in an isolated tree only, and
promotion is a separate decision. The full implementation and measurement record is in
`results/archive/EXP-2026-09-16-075/REPORT.md`; this card summarizes it and does not
duplicate every table.

## Hypothesis

EXP-074 served a request directly only when its whole page-aligned range was already
registered. Any partial overlap with an existing registration was refused and the request
fell back to the pinned slot ring. The EXP-074 caveat probe measured the cost of that rule
on the real model: an exactly repeated 395-token prompt was +15.17 percent with the direct
path, but a novel 396-token prompt was -5.27 percent, because its expert set only partly
overlapped the earlier registrations
(`experiments/EXP-2026-09-16-070-prefill-pinned-source-direct-dma.md`, section
`### Caveat probe (2026-09-16)`).

The hypothesis under test is that a request which only partially overlaps an existing
registration can still be served from the registered pages, by splitting the request at
the registration boundaries, so novel prompts get the direct path too. The pre-registered
promotion metric is the live prefill `prompt_ms` on a novel prompt after a first prompt
registered its ranges, `>= 5 percent` median paired. This card measures only the model-free
part of that hypothesis, byte-exactness and the counter behavior of the split path; it does
not measure the promotion metric.

## Base

- Base report: `results/archive/EXP-2026-09-16-074/REPORT.md` (EXP-074 pinned-source
  direct DMA, the accepted candidate).
- Isolated tree: `work/llama.cpp-exp075-pinsrc-overlap`.
- Build: `build/exp075-pinsrc-overlap`.
- The accepted trees and builds (`work/llama.cpp-integration`,
  `work/llama.cpp-exp062-ring3`, `work/llama.cpp-exp074-pinsrc`,
  `build/exp063-default-runtime`, `build/exp074-pinsrc`) were not modified.

## Mechanism

Changed in the isolated tree only. The exact diff is
`results/archive/EXP-2026-09-16-075/pinsrc-overlap.patch`, 208 lines, 5 hunks, two files:

- `ggml/src/ggml-cuda/common.cuh`
- `ggml/src/ggml-cuda/ggml-cuda.cu`

New env `GGML_EXPERT_PIN_OVERLAP_SPLIT`, default ON in this build. `0` restores the EXP-074
rule (any overlap refuses the whole request and falls back to the ring), so the two
behaviors can be A/B'd with one binary. `GGML_EXPERT_PIN_SOURCE`,
`GGML_EXPERT_PIN_SOURCE_BUDGET_MB` (24576 MiB) and `GGML_EXPERT_PIN_REGISTER_FAIL` are
unchanged.

`ggml_cuda_expert_pin_try_split` walks the request `[src, src+size)` and cuts it at
registration boundaries:

- a sub-run already covered by one registered interval is one `cudaMemcpyAsync` from the
  registered host address, with no new registration (`direct` sub-copy);
- an uncovered sub-run is page-aligned only for the registration, registered under the
  existing budget and 8 GiB `MemAvailable` margin rules, then copied (`registered`
  sub-copy).

The plan registers every uncovered remainder before the first copy is issued, so a refusal
falls back with nothing copied. Sub-copy sizes follow the original unaligned request
boundaries (the scheduler range already includes the `<= 512 B` expert padding,
`ggml-backend.cpp:1702-1703`), so the copies cover `[src, src+size)` exactly: no gap, no
byte outside the request, and the total copied byte count equals the request size. A
registered interval is merged, so a contiguous covered run is inside one registration and
the `cudaMemcpyAsync` range never crosses a registration end. Copies are issued in request
order on the same compute stream.

Classification: `direct` = request fully covered or fully registered with nothing covered;
`split` = request had at least one covered sub-copy and at least one newly registered
remainder; `fallback` = any sub-range refused (register failure, blacklist, budget, low
headroom, overlap in EXP-074 mode). The release log now separates `direct`, `split` and
`fallback`, and reports covered versus registered sub-copies. Teardown is unchanged: the
context destructor does `cudaDeviceSynchronize`, then `cudaHostUnregister` for every
interval, before the loader unmaps the mapping.

## Model-free gate

Harness `test-exp075-pinsrc.cpp` drives `set_tensor_async_pinned_ring` directly and captures
the release counters through the `ggml_log` callback. All runs are sequential, one process
at a time. Raw logs, `/usr/bin/time -v` traces and hash lists are in
`results/archive/EXP-2026-09-16-075/`. Full detail is in the archive report, section 4.

### Real 17886-range full list (hash `5bbf98f3...`)

The range list is `exp070_ranges.txt`, sha256
`8b58328ed61b0bfea88021e7f0a895233a9a56f078fc65a9ababe4da6b3f6f79`, 17886 ranges,
23 237 033 984 bytes (21.6412 GiB).

| run | env | direct / split / fallback | register failures | intervals | bytes released | peak RSS KiB | swap | hash-list sha256 |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| accepted exp062 | none | n/a (ring) | n/a | n/a | n/a | 24110700 | 0 | `5bbf98f3...` |
| split ON | default | 17886 / 0 / 0 | 0 | 17886 | 23311929344 | 24183916 | 0 | `5bbf98f3...` |
| split OFF | `..._OVERLAP_SPLIT=0` | 17886 / 0 / 0 | 0 | 17886 | 23311929344 | 24184180 | 0 | `5bbf98f3...` |
| register fail | `..._REGISTER_FAIL=1` | 0 / 0 / 17886 | 17886 | 0 | 0 | 24111416 | 0 | `5bbf98f3...` |

`5bbf98f3f18049105f5fe7e758a6bda033c67d2fc1accf3bf5b7b528e90dcde8` is the accepted
EXP-045/070 verified hash-list sha256. It is reproduced exactly by the new build (split ON
and split OFF), by the register-fail fallback, and by the accepted `build/exp062-ring3`
with the same harness; all four `rev-hash-*.txt` files hash to it. The direct runs register
23 311 929 344 bytes (page-aligned), identical to EXP-074. The 17886-range list has no
page-level overlap, so both split modes stay entirely on the direct/registered path, and
`bytes still pinned` was 0 in every run. Peak RSS is about 23.06 GiB (raw 24183916 KiB for
split ON) on the full list.

### Scheduler invariant (G5)

`test-exp074-sched.cpp` runs the accepted 395-token MUL_MAT_ID prefill shape through
`ggml_backend_sched`, so the real `ggml-backend.cpp` weights branch calls the ring
interface.

| run | output sha256 |
| --- | --- |
| split ON | `39505119b144ea286e865119b397eecff75578881c24c4aa1cec9bd642e91510` |
| `GGML_EXPERT_PIN_SOURCE=0` | `39505119...` |
| `..._REGISTER_FAIL=1` | `39505119...` |
| `..._OVERLAP_SPLIT=0` | `39505119...` |

`39505119...` is the accepted EXP-038/041/062 scheduler output hash, so the split path does
not change the scheduler output. The hash is taken from the four `sched-*.bin` files in the
archive, which all hash to it (the `SCHED out_sha` line in `sched-*.log` is a separate
16-hex internal digest, not the payload sha256).

### Shifted set B (register A, then request partly overlapping B)

The scenario registers set A = eight 3 MiB experts with 1 MiB gaps, then requests set B:
eight ranges fully inside an expert and seven shifted 3 MiB ranges that start inside one
expert, cross its end into the gap, and end inside the next expert. Byte-exactness is
checked against a filled device image with sentinel guards on both sides.

| run | direct | split | fallback | cover sub-copies | reg sub-copies | intervals | bytes released | bytes still | swap | VmHWM KiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| split ON | 16 | 7 | 0 | 22 | 15 | 1 | 32505856 | 0 | 0 | 511796 |
| split OFF | 16 | 0 | 7 | 0 | 0 | 8 | 25165824 | 0 | 0 | 512048 |
| register fail | 0 | 0 | 23 | 0 | 0 | 0 | 0 | 0 | 0 | 512048 |

- Byte-exact in every mode: `CHECK PASS shifted: device image byte-exact over A and B`,
  sentinel guards unchanged.
- Split ON: no fallback; the seven shifted requests are served by split sub-copies and the
  majority of sub-copies (22 of 37) come from already-registered pages, so the direct path
  extends to the novel set. This is the point of the experiment.
- Split OFF: the seven shifted requests are refused and fall back to the ring, and the
  output is byte-identical. This is the EXP-074 behavior, and it is the negative control
  for the split.
- Register fail: all 23 requests fall back end to end; eight register attempts fail and
  blacklist their intervals, later requests overlap the blacklist and fall back without a
  new attempt; the output is byte-exact.

### Synthetic selftest and teardown

| run | direct / split / fallback | intervals | bytes released | bytes still | checks |
| --- | --- | ---: | ---: | ---: | --- |
| split ON | 5 / 2 / 0 | 3 | 25169920 | 0 | 8 PASS / 0 FAIL |
| split OFF | 6 / 0 / 1 | 4 | 20979712 | 0 | 8 PASS / 0 FAIL |

Cases cover page-disjoint, page-adjacent, contained, partial-overlap (split ON) / refused
(split OFF) and page-aligned-end ranges; the device image is byte-exact, the sentinel guards
and the source mapping are unchanged, `VmPin` returns to its start value and `VmSwap` stays
0. Teardown: `bytes still pinned` was 0 after every backend free, i.e. the context destructor
released every interval registered by the split path. `VmSwap` stayed 0 in every run.

## Live A/B (2026-09-16, fixed build)

This is the live model A/B the model-free card deferred. It runs on the fixed build only: the
first live attempt (`results/archive/EXP-2026-09-16-075/ab/`) aborted on the second, partly
overlapping prompt in all five runs (`cudaMemcpyAsync`, `invalid argument`), and the physical
registration boundary bug behind it was found and fixed before this run
(`results/archive/EXP-2026-09-16-075/REPORT.md`, section `## BUGFIX`). The fixed build links
the fixed `libggml-cuda.so.0.22.0`; the model-free tables above are the pre-fix record. Raw
data for this section is `results/archive/EXP-2026-09-16-075/ab2/`. This is a candidate A/B
only: no default runtime change is proposed, and the promotion decision is Igor's.

### Protocol

- One binary for both arms: `build/exp075-pinsrc-overlap/bin/llama-server`, sha256
  `4aec1132ec4febae68e050aa93a58397dcb09feec4e640f130d8cedcb32bdafb`, linked against the fixed
  `libggml-cuda.so.0.22.0`, sha256
  `27137a1a3ea0308c7712a3c15b944438e8c5ff2741d681f2ec685bd331ecd45a`.
- The only variable is `GGML_EXPERT_PIN_SOURCE`: arm A = `0` (ring only, today's default
  behaviour), arm B = `1` (pinned-source direct DMA, with `GGML_EXPERT_PIN_OVERLAP_SPLIT` at
  its default ON in this build).
- Port 8087, one fresh server per run, the accepted MTP profile, `-c 196608`, EHS off,
  12 threads.
- Per run, in order: `warm` (`request-short.json`), then `p1` = `request.json` (395 prompt
  tokens), `p2` = `results/archive/EXP-2026-09-16-074/ab/request-alt.json` (396 prompt tokens,
  an expert set that only partly overlaps `p1`), `p2b` = the `p2` prompt again, `p1b` = the
  `p1` prompt again.
- Five pairs, run order A/B, B/A, A/B, B/A, A/B (tag order A1 B1 B2 A2 A3 B3 B4 A4 A5 B5).
  Every one of the 50 requests returned HTTP 200 and no run crashed.
- Raw data: `results/archive/EXP-2026-09-16-075/ab2/` (`A1..A5`, `B1..B5`, each with
  `-warm.json`, `-p1.json`, `-p2.json`, `-p2b.json`, `-p1b.json`); run logs
  `results/e075b-<tag>.log`; monitor CSVs `results/e075b-<tag>-mon.csv`.

### prompt_ms per point

Pair `i` couples `A{i}` with `B{i}` regardless of the run order. The paired gain is
`(A - B) / A` in percent, so a positive value means arm B is faster; the median is over the
five pairs.

| point | request | prompt tokens | arm A median ms | arm B median ms | median paired gain | B wins |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `p1` | `request.json` | 395 | 2824.3 | 4149.6 | -43.17% | 0/5 |
| `p2` | `request-alt.json` | 396 | 3458.2 | 2906.8 | +16.85% | 5/5 |
| `p2b` | `p2` again | 396 | 3470.0 | 2652.7 | +23.60% | 5/5 |
| `p1b` | `p1` again | 395 | 2729.8 | 1648.5 | +39.14% | 5/5 |

Per-run values, `pair 1` is the first A/B couple (tags A1 and B1), and so on:

| point | arm | pair 1 | pair 2 | pair 3 | pair 4 | pair 5 | median |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `p1` | A | 2682.8 | 2934.4 | 2721.5 | 3003.1 | 2824.3 | 2824.3 |
| `p1` | B | 4093.7 | 4201.3 | 4213.4 | 4149.6 | 4008.2 | 4149.6 |
| `p2` | A | 3433.2 | 3458.2 | 3719.1 | 3502.2 | 3349.1 | 3458.2 |
| `p2` | B | 3016.6 | 2722.5 | 2906.8 | 2912.2 | 2902.3 | 2906.8 |
| `p2b` | A | 3470.0 | 3527.9 | 3386.9 | 3367.7 | 3528.3 | 3470.0 |
| `p2b` | B | 2613.6 | 2695.2 | 2694.4 | 2652.7 | 2456.0 | 2652.7 |
| `p1b` | A | 2687.2 | 2770.4 | 2729.8 | 2855.3 | 2548.8 | 2729.8 |
| `p1b` | B | 1704.1 | 1648.5 | 1661.3 | 1638.7 | 1633.3 | 1648.5 |

### Content identity

All 40 measured responses returned the same content in both arms for a given prompt. Hashing
the completion text (not the timing fields) gives two distinct outputs:

- `p1` and `p1b` both hash to
  `7dfa7d1eccbcf2b70008864a40d5eb0a0989bfbbf8f8110d371b7c013fbbf2c1`, the same in arm A and
  arm B;
- `p2` and `p2b` both hash to
  `db17abc87bf4ca6a42e6e0f5bde4ed950d7389eea0820f9a3b1e819502a3474a`, the same in arm A and
  arm B.

The split path changes the timing and not the output.

### Release counters (arm B)

Identical in all five arm B runs. Arm A released nothing (`0 direct / 0 split / 0 fallback`,
0 bytes released, 0 bytes still pinned). Swap stayed 0 in every run of both arms.

| counter | arm B value (all 5 runs) |
| --- | ---: |
| direct ranges | 62742 |
| split ranges | 7442 |
| fallback ranges | 14452 |
| register failures | 0 |
| intervals released | 17955 |
| physical regions released | 28968 |
| bytes released | 25 769 775 104 (24.0 GiB) |
| bytes still pinned | 0 |

### Interpretation

- The split removes the EXP-074 overlap fallback, so a novel prompt is now served by the
  direct path. In the EXP-074 caveat probe the partly overlapping `p2` prompt was -5.27%; in
  this run the same class of prompt is +16.85% on first touch (`p2`) and +23.60% warm
  (`p2b`), 5/5 in both cases and byte-identical.
- The repeated prompt is +39.14% (`p1b`, 5/5), on top of the steady-state direct-path result
  already seen in EXP-074.
- The one remaining cost is the first request after a fresh server start: `p1` is -43.17%,
  0/5. That is the one-time registration of about 21.7 GiB of source ranges, and it is
  amortized over the session (the same registration makes `p2`, `p2b` and `p1b` fast).
  Hiding it is a separate follow-up, tracked as `PRE-PIN3` in
  `experiments/HYPOTHESES-2026-09-15-runtime-speed.md`.
- The budget cap is now the binding limit. The 24 GiB (`24576 MiB`) budget was fully consumed
  (released bytes reached the cap) and 14452 of the 84436 copied ranges fell back to the
  ring, so raising the budget is a separate, safety-gated question.
- This is a candidate build, not the default runtime.

### Verdict

- `CONFIRMED_SPLIT`: the novel prompt is now served directly, 5/5 on `p2`, `p2b` and `p1b`,
  byte-identical.
- `COLD_START_COST`: the first request after a fresh server start is -43.17%, 0/5, and it is
  amortized over the session.

No default runtime change is proposed and no promotion is implied. The decision is Igor's, and
promotion would be a separate experiment.

## Safety notes

- The split path reuses the EXP-074 budget (24 GiB process-wide cap) and the 8 GiB
  `MemAvailable` margin per registration; a refusal falls back, never a partial or unsafe
  copy.
- The plan registers all remainders before any copy, so a fallback does not double-copy
  bytes.
- Registration still refuses overlap; the split logic only passes non-overlapping
  page-aligned remainders to it.
- Bounded: the number of sub-copies per request is bounded by the number of registered
  intervals the request intersects.
- No repository tree, accepted build or `work/llama.cpp-integration` was modified by this
  experiment; the patch touches the isolated tree only. Nothing was committed.

## Artifacts

Build (`build/exp075-pinsrc-overlap/bin/`):

| File | sha256 |
| --- | --- |
| `llama-server` | `4aec1132ec4febae68e050aa93a58397dcb09feec4e640f130d8cedcb32bdafb` |
| `libggml-cuda.so.0.22.0` | `214ac0109c886559d81576d7d1038b7063fb14d56b38a79b6da27c26082e0305` |

Gate files in `results/archive/EXP-2026-09-16-075/`:

| File | sha256 |
| --- | --- |
| `pinsrc-overlap.patch` | `b6139b3544be756119c27e7100621a149f62e689a999faa5e09acf6d0fd8dc54` |
| `test-exp075-pinsrc` | `4f1911a829db0418230eb221454069e51239cae205f89e4098bfa71e25b511ef` |
| `test-exp074-pinsrc` | `f0bedac3f03626433b2eb19fd27614b80de0f1e5a022fc107006408777e7f298` |
| `test-exp074-sched` | `d80963c87658587015061a5f528da8e0ea0d96754032034b0030cd0ebe05dba5` |
| `rev-hash-{accepted,split-on,split-off,regfail}.txt` | `5bbf98f3f18049105f5fe7e758a6bda033c67d2fc1accf3bf5b7b528e90dcde8` |

Also present: `test-exp075-pinsrc.cpp`, `test-exp074-pinsrc.cpp`, `test-exp074-sched.cpp`,
`run-{accepted,split-on,split-off,regfail}.{log,time}`, `selftest-{on,off}.txt`,
`shifted-{on,off,regfail}.txt`, `sched-{on,off,fail,splitoff}.{log,bin}`,
`build-baseline.log`, `build-pinsplit.log`, `environment.txt`.

Environment: NVIDIA GeForce RTX 4070 12 GiB, driver 580.178.04, CUDA 12.6 nvcc, g++-13
13.4.0, 63 643 556 kB MemTotal, `RLIMIT_MEMLOCK` 8192 KiB. Full record in
`results/archive/EXP-2026-09-16-075/REPORT.md`.

## Not covered

- The model-free gate runs no model and no server and makes no prefill or decode claim. The
  live A/B of this candidate on the fixed build is reported in the `Live A/B` section above;
  it is a candidate A/B only, no default runtime change is proposed, and promotion needs
  Igor's decision.
- No throughput number is produced here. The expected benefit is bounded by the EXP-074
  caveat probe: the split path should remove the `range overlaps a registered range`
  fallback for novel prompts, but that must be measured with the model.
- The split adds one `cudaMemcpyAsync` per covered/registered run instead of one copy per
  request; the transfer-time effect is not measured here.
