# EXP-2026-09-16-070: prefill expert transfer, pinned-source direct DMA vs the CPU gather

Status: `MODEL-FREE CONFIRMED` (2026-09-16).
Kind: `prototype-offline` (no model, no server; no server throughput claim is made).

This card is a model-free transfer-path experiment. It starts no model and no
`llama-server`, and it makes no aggregate or single-response throughput claim. It is
the written verdict for the candidate tracked as `PRE-PIN` in
`experiments/HYPOTHESES-2026-09-15-runtime-speed.md`. It is not an accepted runtime
change; promoting the direction needs a model-level A/B, which was run afterwards and is
recorded in the Model A/B section at the end of this card.

## Hypothesis

The EXP-2026-09-07-043 profile of the accepted runtime named the CPU gather (1463.6 ms
union, 56.9 percent of the measured 395-token prefill window) as the dominant term of
the pinned-ring path. The current path copies each selected expert range out of the
pageable GGUF mapping into a 16 MiB pinned slot on the CPU, then issues
`cudaMemcpyAsync` from the slot to the device (ring depth 3, one stream). Two claims
were pre-registered:

1. The CPU gather of selected expert ranges into pinned slots is removable. Pinning
   the source ranges once and issuing direct device copies should remove the gather
   term entirely.
2. A dedicated copy stream should raise effective bandwidth by decoupling the H2D
   copies from the compute stream's ordering.

Both claims are tested by one harness over the real EXP-045 range list.

## Arms

- Arm A (current): two-thread CPU gather into 3 x 16 MiB pinned slots plus H2D, one
  stream, ring depth 3 (`GGML_CUDA_EXPERT_RING_SLOTS = 3`, the delivered EXP-062
  depth). The gather helper is one persistent thread created outside the timed passes.
- Arm B (pinned source): the source ranges are pinned once with `cuMemHostRegister`;
  the timed loop issues one `cuMemcpyHtoDAsync` per range directly from the registered
  source pointer to the device, with no CPU gather and no staging slot.
- Arm C (dedicated copy stream): identical to A, but the copies and their per-slot
  events are on a second `copy_stream`, and after each copy the compute stream executes
  `cuStreamWaitEvent` on the copy event. With no consumer kernel in the model-free
  harness this only models the dependency edge; C therefore isolates stream separation
  without any compute overlap.

Metrics: the full gather plus H2D sequence with a final drain (the EXP-045 primary
metric), the achieved H2D bandwidth, byte parity, and `VmSwap`.

## Protocol

- The real EXP-045 range list is reused verbatim: `exp070_ranges.txt`, sha256
  `8b58328ed61b0bfea88021e7f0a895233a9a56f078fc65a9ababe4da6b3f6f79` (identical to the
  EXP-045 file). 17 886 ranges, 23 237 033 984 bytes (21.6412 GiB), min 524 800 B,
  max 11 981 312 B, spanning GGUF shards 2 and 3.
- Shards are mapped `MAP_PRIVATE PROT_READ`; the destination is a 64 MiB wrapping
  device arena with the same cursor logic as EXP-045.
- 8 rounds. Rounds r1 to r4 ran A, B, C; rounds r5 to r8 ran C, B, A (reversed) to
  cancel ordering effects.
- Each invocation is warmup 1 plus 1 measured pass. All arms move 17 886 chunks and
  23 237 033 984 bytes.

## Results

Per-arm medians over the 8 rounds:

| Arm | n | wall median ms | wall min..max | achieved bw median GiB/s | gather median ms | slotwait median ms |
| --- | ---: | ---: | --- | ---: | ---: | ---: |
| A (3 slots, current) | 8 | 1923.2 | 1732.0 .. 2008.0 | 11.25 | 1763.5 | 107.9 |
| B (pinned source) | 8 | 1000.7 | 986.3 .. 1032.5 | 21.63 | 0.0 | 0.0 |
| C (copy stream) | 8 | 1891.9 | 1665.4 .. 2020.5 | 11.44 | 1732.7 | 107.8 |
| A2 (2 slots, depth control) | 4 | 1990.8 | 1959.0 .. 2044.1 | 10.87 | 1577.5 | 361.6 |

Paired deltas:

- B vs A: median paired delta **-47.72 percent**, B wins **8/8** rounds (per-round
  -47.9, -48.6, -47.3, -47.6, -48.8, -47.2, -42.2, -50.1 percent).
- C vs A: median paired delta **+0.49 percent**, C wins **4/8**. The per-round deltas
  change sign with run order (-14.5, -16.6, -0.1, +1.0, +3.0, +6.4, +7.4, -3.6
  percent), so the apparent C advantage is a run-order artifact, not an effect of the
  copy stream.
- Effective bandwidth (bytes / full-path wall): A 11.25 GiB/s (12.08 GB/s), C 11.44
  GiB/s (12.28 GB/s), B 21.63 GiB/s (23.23 GB/s). Arm B reaches the pure H2D ceiling
  of this link, about 74 percent of the 31.5 GB/s PCIe gen4 x16 theoretical. Arm C does
  not raise the achieved H2D bandwidth.
- Spread: B 986.3 to 1032.5 ms (4.6 percent of median); A 1732.0 to 2008.0 ms (14.4
  percent). Removing the CPU gather also removes most of the path's sensitivity to CPU
  contention.

Depth control: Arm A with 2 slots (matching the task wording) measured 2044.1, 1959.0,
1997.2, 1984.5 ms (median 1990.8 ms). The slot count is not the limiter; the gather is.

## Correctness

- `--selftest` (5/5 PASS): per-chunk `memcmp` of the staged slot against the source on
  A and C, identical chunk hashes A == B == C, source unchanged, device round-trip
  equal to the source. Cases cover 1, 63, 64, 65, 127, 524800, 524801, 16 MiB-1,
  16 MiB, 16 MiB+1, 16 MiB+65, 17 MiB, 33 MiB, unaligned offsets 0/1/3, and three
  repetitions (slot reuse).
- Full real-list parity: `--hash` over all 17 886 chunks for A, B and C produced
  identical hash lists, sha256
  `5bbf98f3f18049105f5fe7e758a6bda033c67d2fc1accf3bf5b7b528e90dcde8` (17 886 hashes
  plus header), which is exactly the EXP-045 verified hash-list sha256.
- `VmSwap = 0` in every measured run.

## Pin feasibility for Arm B

`cuMemHostRegister` of the full source succeeds despite the 8 MiB memlock and no
`CAP_IPC_LOCK`: 17 886 intervals, 23 311 929 344 bytes (21.7109 GiB, 71.4 MiB
page-alignment overhead), 0 failures, `complete = true` (`pinprobe-full.txt`). Cold
registration cost is about 36 s (one-time page-in plus pinning). No smaller-window
fallback was needed.

Two environment facts mattered:

- `RLIMIT_MEMLOCK` is 8192 KiB and `CapEff` has no `CAP_IPC_LOCK`. The registration
  still succeeds on this driver, so the 8 MiB limit is not the effective cap for
  `cuMemHostRegister` here.
- `MAP_SHARED` file mappings cannot be registered with the driver
  (`CUDA_ERROR_INVALID_VALUE`). Arm B therefore uses `MAP_PRIVATE PROT_READ` plus
  `CU_MEMHOSTREGISTER_READ_ONLY`; all arms read the same pages.

## Verdict

- **Arm B: CONFIRMED at the model-free level.** Removing the gather cuts the full
  gather+H2D+drain by 47.72 percent (median 1923.2 to 1000.7 ms), far above the >= 5
  percent promotion threshold, with 8/8 wins, byte parity and `VmSwap = 0`. This
  passes the model-free promotion gate by a wide margin, and the direction is worth a
  model-level integration experiment.
- **Arm C: REFUTED (null).** A dedicated copy stream with no compute overlap is
  statistically indistinguishable from the current path (+0.49 percent paired median,
  4/8 wins) and does not raise the achieved H2D bandwidth. Stream separation alone is
  not a lever.

## Explicit non-claims

- This is a model-free measurement of the expert-transfer path only. It is not a
  server throughput claim, and it is not an aggregate tok/s claim.
- No model, no `llama-server` and no profiler was started for this experiment.
- The integration of Arm B needs a model-level A/B. It was not run here and is not
  authorized by this card.

## Integration caveats (not measured here)

1. Arm B pins the whole source working set: 21.7109 GiB of non-pageable RAM for this
   single 395-token prompt. On the 64 GiB host this fits, but pinning is not
   reclaimable and interacts with swap safety (`VmSwap` must stay 0).
2. In production the selected expert ranges change per prompt and per token; pinning
   on first touch means the pinned set grows over a session. Over many distinct prompts
   the union can approach the whole 82 GB model, which cannot be pinned on this host. A
   bounded pinned window (direct-copy the ranges inside the window, gather the rest) is
   the obvious mitigation but was not measured here.
3. `cuMemHostUnregister` or process exit releases the pins; nothing in this harness
   changes the accepted runtime or its builds.

## Files and commands

Artifacts live in `results/archive/EXP-2026-09-16-070/`:

| File | sha256 |
| --- | --- |
| `exp070_transfer3.cpp` | `0d3bee424f0d561eb4d07c5c2aff56152eaa020e5cd837fa4dc2bd18681d8ba7` |
| `exp070_pinprobe.cpp` | `d1221c1021af9cf36ae9c7371e6bc490836668d5675a4405c39a93ede9cebefb` |
| `exp070_ranges.txt` | `8b58328ed61b0bfea88021e7f0a895233a9a56f078fc65a9ababe4da6b3f6f79` |
| `gen_exp070_ranges.py` | `864b7d74e9d338f7a17baae8436d2a85239ee53b962b98c74b551ceda3326c39` |
| `build.sh` | `19853de0ad0e0611b353101e5afd27edd53c5fe64eeb9835391f9b86d30cf5e3` |
| `run_arm.sh` | `fc5921c66a17d67e6bb0f749c414185762ba97fe470246a3fd94823c1c98ba71` |

Also present: `selftest.txt`, `pinprobe-full.txt`, `environment.txt`, `summary.json`,
`run-{A,B,C}-r1..r8.json`, `run-A2s-r1..r4.json` (raw per-run telemetry), and
`rev-hash-{A,B,C}.txt` (full-list hash verification, identical to EXP-045).

Built binaries in the gitignored `build/exp070-transfer3/`: `exp070_transfer3` sha256
`c59c359a6b901c8dc074a073021440790b521a7ce7fb1651af7d80df3b9b6fe9`, `exp070_pinprobe`
sha256 `2dd3cc97a219b3eaa9e13cee8b2ce376ee2899c3ecdb5b087ed7d58935425d96`.

Environment: NVIDIA GeForce RTX 4070 12 GiB, driver 580.178.04, PCIe gen4 x16, AMD
Ryzen 9 5950X (32 threads), 63 643 556 kB MemTotal, `RLIMIT_MEMLOCK` 8192 KiB, glibc
2.43, g++-13 13.4.0. The local CUDA 12.6 `nvcc` cannot compile a `.cu` file against
glibc 2.43 (`rsqrt`/`cospi`/`sinpi` exception-specification clash), so the harness
targets the CUDA Driver API (`cuda.h`) and is built with a plain host compiler; no
device code is needed for a memcpy-only workload.

Commands (already executed; do not re-run from this card):

```bash
# build (one build directory, own tree; accepted builds untouched)
results/archive/EXP-2026-09-16-070/build.sh

# correctness
build/exp070-transfer3/exp070_transfer3 --selftest
build/exp070-transfer3/exp070_pinprobe --ranges .../exp070_ranges.txt --shard <3 shards>

# one measured run of one arm, strictly sequential (warmup 1 + 1 measured pass)
./run_arm.sh A A-r1 --passes 1
./run_arm.sh B B-r1 --passes 1
./run_arm.sh C C-r1 --passes 1

# full-list byte parity
build/exp070-transfer3/exp070_transfer3 --ranges .../exp070_ranges.txt --shard <3 shards> \
    --mode A --slots 3 --warmup 0 --passes 1 --hash rev-hash-A.txt   # and B, C
```

## Modification and commit note

No repository tree, patch, accepted build or `work/llama.cpp-integration` was modified
by this experiment. Nothing was committed. The measurement artifacts are appended
files under `results/archive/EXP-2026-09-16-070/` only.

## Model A/B (2026-09-16)

This section records the live model-level A/B that the model-free part of this card left
open. It measures the pinned-source direct-DMA direction on the real model and server,
using the candidate build produced by the EXP-074 integration
(`results/archive/EXP-2026-09-16-074/REPORT.md`). It is a candidate-build measurement
only: no default runtime change is proposed, the accepted daily runtime is untouched, and
promoting the direction is Igor's decision (Hard rule 10).

### Arms and single-variable control

- One binary for both arms: `build/exp074-pinsrc/bin/llama-server`, sha256
  `9da1c3ef5840234a9d2252d84ba4c62f13e0a0cb3b6570b65ae677d0ff2a8093`, the EXP-074 build.
- The only variable is the env `GGML_EXPERT_PIN_SOURCE`:
  - Arm A = `0`: the accepted pinned ring only; the direct-copy branch is off.
  - Arm B = `1`: pinned-source direct DMA (the default value of the flag in this build).
- Port 8087, one fresh server per run, 12 CPU threads, EHS off, the accepted MTP profile,
  `-c 196608`.

### Protocol

- Per run: one warmup `request-short.json`, then `m1` = the 395-token request, then `m2` =
  the same 395-token request again.
- For arm B, `m1` pays the one-time registration of the expert ranges it selects; `m2` is
  steady state, where those ranges are already registered and copied directly.
- Five pairs, order A/B, B/A, A/B, B/A, A/B to cancel ordering effects.
- Metrics: `prompt_ms` from the server timing block (`prompt_n = 395` in every measured
  request), the completion content sha256, and the pin-source activation log line.

### m1: first request (arm B pays the registration)

| pair | A prompt_ms | B prompt_ms | paired gain (A-B)/A |
| ---: | ---: | ---: | ---: |
| 1 | 3190.1 | 3760.5 | -17.88 percent |
| 2 | 2650.6 | 3462.5 | -30.63 percent |
| 3 | 3120.3 | 3892.5 | -24.75 percent |
| 4 | 2763.3 | 3708.4 | -34.20 percent |
| 5 | 3006.2 | 3814.4 | -26.88 percent |
| median | 3006.2 | 3760.5 | **-26.88 percent** |

Arm B is slower on every pair: **0/5 wins** for B.

### m2: steady state (ranges already registered)

| pair | A prompt_ms | B prompt_ms | paired gain (A-B)/A |
| ---: | ---: | ---: | ---: |
| 1 | 2776.0 | 2325.4 | +16.23 percent |
| 2 | 2827.9 | 2321.2 | +17.92 percent |
| 3 | 2915.8 | 2393.0 | +17.93 percent |
| 4 | 2980.2 | 2226.3 | +25.30 percent |
| 5 | 2917.4 | 2352.1 | +19.38 percent |
| median | 2915.8 | 2325.4 | **+17.93 percent** |

Arm B is faster on every pair: **5/5 wins** for B, above the >= 3 percent paired-prefill
bar used across the PRE series.

### Correctness and activation

- The completion content sha256
  `7dfa7d1eccbcf2b70008864a40d5eb0a0989bfbbf8f8110d371b7c013fbbf2c1` is identical in all
  20 measured runs (both arms, `m1` and `m2`). The direct path is output-preserving on this
  workload.
- Arm B logs
  `expert pinned source enabled: direct H2D from the mapped GGUF (budget 24576 MiB)`, once
  per server: the ring/pin path is active, and the ring is bypassed for registered ranges.

### Budget caveat

One 395-token prefill registers about 21.7 GiB of source ranges, close to the default
24 GiB (`GGML_EXPERT_PIN_SOURCE_BUDGET_MB = 24576`) process-wide cap. The cap is a real
ceiling, but the probe below shows that the dominant limit is not the byte cap or the
budget arithmetic: it is the registration overlap rule. A range that is exactly contained
in an existing registration is a direct hit; a range that only partially overlaps or
extends past a registered interval is refused and falls back to the ring, even though the
already-registered pages could serve the copy. The steady-state +17.93 percent therefore
applies only to the part of the working set that is already pinned and requested again
exactly, and the sign of the net benefit over a multi-prompt session depends on how much
the selected expert set repeats exactly, which the A/B above did not characterize (the
probe below does).

### Caveat probe (2026-09-16)

Same binary (`build/exp074-pinsrc/bin/llama-server`, sha256 `9da1c3ef...`) for both arms,
port 8087, one fresh server per arm. Per run: warmup `request-short.json`, then `p1` = the
395-token `request.json`, then `p2` = a different 396-token prompt
(`results/archive/EXP-2026-09-16-074/ab/request-alt.json`, which selects different
experts), then `p1` again as `p1b`. Arm V2 = `GGML_EXPERT_PIN_SOURCE=1` (pinsrc), arm
W2 = `GGML_EXPERT_PIN_SOURCE=0` (ring only). Both arms are single fresh-server runs, so
these are point measurements, not a paired median.

| request | tokens | V2 pinsrc prompt_ms | W2 ring prompt_ms | V2 content sha256 |
| --- | ---: | ---: | ---: | --- |
| `p1` (first, registers its experts) | 395 | 4952.6 | 3119.4 | `7dfa7d1eccbcf2b7...` |
| `p2` (different prompt, different experts) | 396 | 3926.4 | 3729.8 | `db17abc87bf4...` |
| `p1b` (same prompt again) | 395 | 2461.9 | 2902.0 | `7dfa7d1eccbcf2b7...` |

Derived paired numbers:

- `p1b` (repeated prompt, third request): pinsrc 2461.9 ms vs ring 2902.0 ms, pinsrc
  **+15.17 percent** faster. Once the ranges are registered and requested again exactly,
  the direct path wins.
- `p2` (novel prompt, different experts): pinsrc 3926.4 ms vs ring 3729.8 ms, pinsrc
  **-5.27 percent** slower. The novel expert set cannot reuse the registrations made by
  `p1`, so it falls back to the ring and still pays the registration attempt.

Release counters from the V2 server log (`results/pinsrc-V2.log`):
`expert pinned source: 44442 direct ranges, 22365 fallback ranges, 0 register failures,
21963 intervals released`. Over the three requests roughly two thirds of the copies went
direct and one third fell back.

Fallback reason in the same log (a reason not present in the EXP-074 gate report):
`pinned source unavailable (range overlaps a registered range), using the pinned slot ring`.

Interpretation: only an exact containment is served directly; a partially overlapping or
extending range is refused even when the covering registration already holds its pages.
Over a multi-prompt session the net benefit therefore depends on how much expert ranges
repeat exactly, not merely on how much they overlap. This is the concrete, actionable
limitation of the current candidate, and it names the natural next hypothesis: serve
partially overlapping ranges from the covering registration (split the request across the
covering interval, or extend the interval), which would extend the direct path to novel
prompts. That follow-up is recorded as `PRE-PIN2` in
`experiments/HYPOTHESES-2026-09-15-runtime-speed.md` and is not measured here.

### Verdict

- **CONFIRMED_POST_PIN.** Once the ranges are registered, the pinned-source direct-DMA
  path cuts `m2` prefill by a median **+17.93 percent** with **5/5** wins, above the
  >= 3 percent bar, with byte-identical completion content. The steady-state gain is
  confirmed at the model level.
- **REJECTED_COLD_START.** The first request that pays the registration is a median
  **-26.88 percent** regression with **0/5** wins, because the lazy registration sits on
  the first-request critical path. The cold-start regression is recorded as a real cost of
  this design, not a noise artifact.
- **Overlap fallback is why the net sign depends on repetition.** The caveat probe shows
  the limit is the registration overlap rule, not the byte budget: a novel prompt that
  only partially overlaps existing registrations falls back to the ring (`p2`,
  -5.27 percent), while an exactly repeated prompt wins (`p1b`, +15.17 percent). The
  multi-prompt net sign therefore depends on how much the expert set repeats exactly.
- **No default runtime change is proposed.** Both statements are true at once: this build
  is a candidate, not the default. Whether the steady-state win outweighs the cold-start
  cost, and whether the overlap fallback makes the win workload-dependent, is Igor's
  decision.

### Evidence

- Raw responses: `results/archive/EXP-2026-09-16-074/ab/`, `A1..A5` and `B1..B5`, each with
  `-warmup.json`, `-m1.json` and `-m2.json`. Every measured file reports `prompt_n = 395`.
- Run logs: `results/pinsrc-A1..A5.log`, `results/pinsrc-B1..B5.log`.
- Implementation and model-free gate for the same binary:
  `results/archive/EXP-2026-09-16-074/REPORT.md`.
