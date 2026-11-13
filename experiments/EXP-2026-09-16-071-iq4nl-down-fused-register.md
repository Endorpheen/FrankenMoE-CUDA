# EXP-2026-09-16-071: register-resident fused IQ4_NL down projection at the decode verify shape

Status: kernel `MODEL-FREE CONFIRMED` (2026-09-16); the integrated end-to-end decode A/B is
`REJECTED` (see the Decode A/B section at the end).
Kind: `microbenchmark`.

This card is a model-free kernel microbenchmark. It starts no model, no
`llama-server` and no profiler, and it claims no decode throughput change. It is the
written verdict for the candidate tracked as `DEC-IQ4NLREG` in
`experiments/HYPOTHESES-2026-09-15-runtime-speed.md`.

## Hypothesis

At the decode verify shape the down projection runs `MUL_MAT_ID` with IQ4_NL weights at
`ne00 = K = 640`, `ne01 = rows = 2560`, 512 experts, top-10, and `Ny` activation rows
(`Ny = 3`). EXP-048 attributes about 20 to 27 percent of the decode step to this path
and records that it was never optimized. The hypothesis: unpack each weight block once
and apply it to all `Ny` rows in the same iteration, with per-token accumulator
registers, instead of calling the opaque library `vec_dot` once per (row, token).

## Pre-registered bar

Stated before the first run: bit-exact outputs (`maxdiff == 0`) AND `>= 1.15x` versus
the stock library `ggml_vec_dot_iq4_nl_q8_0` at `Ny = 3`. If the bar did not pass, the
conclusion would be that DEC-2/DEC-5 should be closed, and the closing number would be
the `Ny = 3` ratio.

## Variants

- `stock`: library `ggml_vec_dot_iq4_nl_q8_0`, one call per (row, token), through the
  type function pointer.
- `inline`: control, the same AVX2 arithmetic inlined, still one call per (row, token),
  no reuse. Separates the opaque-call cost from the reuse gain.
- `fusB`: the EXP-021 buffer-materialised variant. Unpack the row once into a per-row
  buffer, then apply it to every token with store/load through the buffer.
- `fusR`: register-resident reuse. Unpack each 32-block once and apply it to every
  token in the same iteration, with per-token accumulator registers. This is the
  literal "unpack once, apply to all Ny rows" variant and the strongest ALU-side
  candidate.

The dequant/apply arithmetic is copied from the AVX2 `ggml_vec_dot_iq4_nl_q8_0`
(`arch/x86/quants.c:3920`), split at the weight-only / per-token boundary. Per-token
accumulation order is identical to stock, which is why all variants are bit-exact.

## Setup

- Machine: AMD Ryzen 9 5950X, single pinned core via `taskset -c 8`, 32 MiB L3 per CCD,
  64 MiB total.
- Compiler: `gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0`, flags `-O3 -march=native -std=gnu11`.
- Stock kernel: `ggml_vec_dot_iq4_nl_q8_0` linked from the accepted build
  `build/exp062-ring3/bin/libggml-cpu.so.0.22.0`
  (`sha256 c80ad356daae49e4515c2c54b70f366bb611a8b6b1f50e4e3591f1b9c7825d6a`), tree
  `work/llama.cpp-exp062-ring3` at its patched working state.
- Harness: adapted from `benchmarks/exp021_fused_bench.c`
  (`sha256 d91cc9997d9eb47e250524d870d12658072f6c8e542ea8335b7a51f0a9c68f1f`).
- Activation rows are quantized to Q8_0 once and shared by every variant; the fp32
  activation block scales are precomputed once per token.
- Each row/token set is timed as `NREP = 5` measured repetitions plus one warmup; the
  median is reported. Every variant processes bit-identical weight bytes.

The stock path is faithful to the real code: `ggml_compute_forward_mul_mat_id_one_chunk`
(`ggml/src/ggml-cpu/ggml-cpu.c:1520`) calls the opaque `vec_dot` function pointer once
per (row, token) with `nrc = 1`. The only existing fused `mul_mat_id` path is for IQ2_S
(`ggml-cpu.c:1936`); IQ4_NL has none.

Methodological note: the first harness build converted fp16 scales through the
`ggml_fp16_to_fp32` function, which is a PLT call in the hot loop; the library uses the
`ggml_fp16_to_fp32` table lookup. That penalty applied once per weight block to
`fusB`/`fusR` and to `inline`, while stock used the table. After switching the harness
to the same table lookup the `Ny = 3` `fusR` ratio moved from about 1.18x to about
1.62x. EXP-021's fused numbers carried the same penalty, so its low-`Ny` conclusion is
partly an artifact of the harness, not only of the algorithm.

## Results

One call is one expert's down projection: 2560 rows x `Ny` tokens. Weight bytes per call
is 921 600. Times are ns/call, median of 5. The `x` ratios are stock/variant, so a value
above 1 means the variant is faster than stock.

### Resident table (run 1, working set 0.88 MiB, fits L3)

`exp071-down-iq4nl.txt`. K = 640, rows = 2560.

| Ny | stock ns | inline ns | fusB ns | fusR ns | inl x | fusB x | fusR x | maxdiff | verdict |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | :-- |
| 1 | 79941 | 72615 | 95487 | 79671 | 1.101 | 0.837 | 1.003 | 0.0e+00 | FAIL |
| 2 | 158368 | 143638 | 149337 | 112681 | 1.103 | 1.060 | 1.405 | 0.0e+00 | PASS |
| 3 | 238401 | 214583 | 202715 | 146536 | 1.111 | 1.176 | **1.627** | 0.0e+00 | **PASS** |
| 4 | 318221 | 287550 | 260729 | 192135 | 1.107 | 1.221 | 1.656 | 0.0e+00 | PASS |
| 8 | 633289 | 570748 | 472005 | 351173 | 1.110 | 1.342 | 1.803 | 0.0e+00 | PASS |

### Streaming table (run 2, working set 112 MiB, above L3)

`exp071-down-iq4nl-run2.txt`. Single-core read-sum ceiling on the same buffer: 21.64
GiB/s (23.233 bytes/ns). K = 640, rows = 2560.

| Ny | stock ns | inline ns | fusR ns | inl x | fusR x | stock B/ns | fusR B/ns | maxdiff | verdict |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | :-- |
| 1 | 821921 | 754665 | 838370 | 1.089 | 0.980 | 11.21 | 10.99 | 0.0e+00 | FAIL |
| 2 | 1696864 | 1439940 | 1133336 | 1.178 | 1.497 | 5.43 | 8.13 | 0.0e+00 | PASS |
| 3 | 2376734 | 2214523 | 1462681 | 1.073 | **1.625** | 3.88 | 6.30 | 0.0e+00 | **PASS** |
| 4 | 3196295 | 2854994 | 1974245 | 1.120 | 1.619 | 2.88 | 4.67 | 0.0e+00 | PASS |
| 8 | 6351076 | 5730350 | 3544187 | 1.108 | 1.792 | 1.45 | 2.60 | 0.0e+00 | PASS |

`maxdiff == 0` in every cell of both tables, so every variant is bit-exact against the
stock library output.

### Reproducibility

The two independent runs of the same binary cross-check each other at `Ny = 3`:

| Case | fusR x | source |
| --- | ---: | --- |
| resident run 1 | 1.627 | `exp071-down-iq4nl.txt` |
| resident run 2 | 1.620 | `exp071-down-iq4nl-run2.txt` |
| streaming run 1 | 1.621 | `exp071-down-iq4nl.txt` |
| streaming run 2 | 1.625 | `exp071-down-iq4nl-run2.txt` |

Run-to-run spread at `Ny = 3` is under 0.5 percent. Pure reuse gain (`fusR` versus the
inlined no-reuse control) at `Ny = 3` is 1.464x resident and 1.460x streaming in run 1.

### Where the crossover is

`fusR` has no loss region at this shape: at `Ny = 1` it is parity (0.98x to 1.02x), and
it wins for every `Ny >= 2` (about 1.40x to 1.50x at `Ny = 2`). The crossover from "not
worse" to "clearly better" is between `Ny = 1` and `Ny = 2`. The EXP-021 buffer variant
`fusB` crosses between `Ny = 1` (0.84x) and `Ny = 2` (1.06x).

### Bytes/ns and the DRAM ceiling

The read-sum ceiling of one pinned core is 21.6 to 22.8 GiB/s (23.2 to 24.5 bytes/ns).
At `Ny = 3`, stock delivers only 3.88 bytes/ns of unique weight traffic and `fusR` 6.28
bytes/ns, about 16 percent and 26 percent of the ceiling. The L3-resident and DRAM-sized
tables produce the same per-expert time (about 242.7 us versus 244.2 us per expert for
stock at `Ny = 3`), so the kernel is not bandwidth-bound: it is ALU/issue-bound. That is
why an ALU-side reuse change is the correct lever here, and why the result is stable
across cache regimes.

Sanity cross-check: stock at `Ny = 3` is 1.55 ns per 32-element block, consistent with
the EXP-019 measurement of the same library kernel (1.22 to 1.66 ns/block across
regimes).

## Verdict

**PASS, CONFIRMED at the model-free level.** At the real down shape and `Ny = 3` the
register-resident `fusR` variant is **1.627x** faster than the stock library `vec_dot`
on the L3-resident working set (run 1) and **1.625x** on the DRAM-sized working set
(run 2), with `maxdiff == 0` in every case. The pre-registered bar (bit-exact AND
`>= 1.15x` at `Ny = 3`) passes with the register-resident variant.

The buffer variant `fusB` reaches only 1.176x at `Ny = 3`, so the earlier assumption
that IQ4_NL is closed, based on EXP-021, is corrected: EXP-021 measured the buffer
variant and carried the `ggml_fp16_to_fp32` hot-loop penalty, so its low-`Ny`
conclusion does not hold for the register-resident variant.

## Caveats

1. Scope is the kernel work of one full down `MUL_MAT_ID` call. The real call
   additionally pays per-expert dispatch, gather and scheduling; those are not removed
   by the fused kernel and would dilute the end-to-end gain. The 1.62x is a kernel-work
   ratio, not a full-call wall-time ratio.
2. There is no gain at `Ny = 1` (0.98x to 1.02x, parity). Reuse requires at least two
   tokens to share one expert. With 3 verify tokens x top-10 and 512 experts, an expert
   selected by only one token gives no reuse. The realized end-to-end effect therefore
   depends on the actual per-expert group-size distribution of the down projection at
   decode, which is not yet measured. EXP-048 reports near-full 2 to 3 row sharing for
   this projection, which is exactly the region measured here, but that is an
   attribution, not a group-size measurement.
3. The streaming table reuses a rotating window of 128 experts; this makes the working
   set 113 MiB (above L3), but the kernel is compute-bound, so it does not change the
   conclusion.
4. No model was run. This microbenchmark neither proves nor bounds any decode
   throughput change, and no decode throughput is claimed.

## Files and commands

All artifacts are under `results/archive/EXP-2026-09-16-071/`:

| File | sha256 |
| --- | --- |
| `exp071_down_iq4nl_bench.c` | `86ae59cce7e84c6978dcabd81523718fda542486507b196d3cf2a6ab756ea647` |
| `exp071_down_iq4nl_bench` | `21eae5ed4537f0ac9ea298c6beb2422186d2db437c6cd107f76fc9b2c1751f58` |
| `exp071-down-iq4nl.txt` | `11f3c15f5000de57adb1843b19b3143d4d40a9b6e3ab7be6837c3aa5f601eadc` |
| `exp071-down-iq4nl-run2.txt` | `55e229dcb312ef5f6e8e065161ae115a654441892d9d5b620bf6b2d247e8e2d8` |
| `exp071-stderr.txt` | `610aec6dfadc659ab967b637be0fe2a958827d65748f46c7e1957bc14f3b507b` |
| `exp071-stderr-run2.txt` | `610aec6dfadc659ab967b637be0fe2a958827d65748f46c7e1957bc14f3b507b` |

Commands (already executed; do not re-run from this card):

```bash
# 1. create the artifact directory
mkdir -p results/archive/EXP-2026-09-16-071

# 2. build the harness against the accepted build's stock vec_dot
gcc -O3 -march=native -std=gnu11 -Wall -Wextra \
  -o results/archive/EXP-2026-09-16-071/exp071_down_iq4nl_bench \
  results/archive/EXP-2026-09-16-071/exp071_down_iq4nl_bench.c \
  -I work/llama.cpp-exp062-ring3/ggml/include \
  -L build/exp062-ring3/bin -lggml-cpu -lggml-base -lm \
  -Wl,-rpath,/home/end0/AI/FrankenMoE-CUDA/build/exp062-ring3/bin

# 3. run pinned to one core, twice
cd results/archive/EXP-2026-09-16-071
taskset -c 8 ./exp071_down_iq4nl_bench > exp071-down-iq4nl.txt 2> exp071-stderr.txt
taskset -c 8 ./exp071_down_iq4nl_bench > exp071-down-iq4nl-run2.txt 2> exp071-stderr-run2.txt
```

## Modification and commit note

No tracked file in the repository, and no file of `work/llama.cpp-exp062-ring3` or
`build/exp062-ring3`, was modified by this experiment. Nothing was committed. The
harness and its raw output are appended files under
`results/archive/EXP-2026-09-16-071/` only.

## Decode A/B (2026-09-16)

Status: `REJECTED` at the model level. The kernel result above stands as a documented
microbenchmark; this section records the end-to-end test of the integrated path.

The fused kernel was then integrated into a candidate server tree and measured live. The
integration itself is documented in `results/archive/EXP-2026-09-16-072/REPORT.md`; raw A/B
JSON is in `results/archive/EXP-2026-09-16-072/ab/`.

### Protocol

- One binary for both arms: `build/exp072-iq4nl-fused/bin/llama-server`, sha256
  `38b17c7f6832e554f7176e7d86869172c9fd77f8e001892f1b1590299466d4fd`.
- The only variable is the env `GGML_CPU_IQ4NL_FUSED`: arm A sets it to `0` (fused path off),
  arm B sets it to `1` (fused path on) plus `GGML_CPU_IQ4NL_FUSED_STATS=1` for the counters.
  Everything else is identical between arms.
- Port 8087, 12 threads, EHS off, same accepted runtime profile. Each fresh server runs one
  warmup `request-short.json` and then one measured `request-decode256.json`.
- Arm order across the 5 pairs: A/B, B/A, A/B, B/A, A/B.
- One warm run was executed first and excluded from the analysis; it is kept as
  `warm-decode256.json` (15.3840 tok/s) and is not part of any pair.

### Results

Measured `predicted_per_second` for the decode256 request, one value per fresh server.

| Pair | Arm A (off) | Arm B (on) | Paired gain B vs A |
| ---: | ---: | ---: | ---: |
| 1 (A/B) | 18.6129 | 19.1672 | +2.978% |
| 2 (B/A) | 18.9070 | 18.5676 | -1.795% |
| 3 (A/B) | 18.6560 | 18.8344 | +0.956% |
| 4 (B/A) | 18.9287 | 18.4261 | -2.655% |
| 5 (A/B) | 18.7295 | 19.1649 | +2.325% |

- Median paired gain: **+0.956%**, B wins **3/5**.
- Arm medians: A **18.7295** tok/s, B **18.8344** tok/s, a **+0.560%** difference.
- The completion `content` sha256
  `9270353f0601d5d660a61ee72b94ece46c5d7ed425321da8e88bd09f1abd9757` is identical in all 10
  measured runs, so the fused path is byte-preserving end to end; the reject is purely about
  speed.

### Verdict

**REJECTED at the model level.** There is no measurable decode gain: the median paired gain
+0.956% is below the `>= 2%` bar and inside the observed run-to-run spread, which spans about
-2.7% to +3.0% with alternating signs around zero.

The model-free kernel ratios from EXP-071 and the EXP-072 full-call bench (1.627x at `Ny = 3`
with full sharing, 1.1485x at 12 threads with full sharing, 1.0519x on the EXP-046-like routing
mix, 1.0131x on sparse routing) predicted a routing-dependent end-to-end effect of roughly 1 to
3%. The real decode routing does not provide enough expert sharing for that to clear the bar,
which is exactly the risk noted in the Caveats above: the realized group-size distribution is
not the near-full sharing of the model-free cases.

The kernel microbenchmark (1.627x at `Ny = 3`, bit-exact) is unaffected and remains a valid,
documented result. What is rejected is the claim that integrating it into the decode path
produces a measurable end-to-end decode speedup. The fused path stays default off in the
candidate build; the accepted runtime is unchanged.

### Files

| File | sha256 |
| --- | --- |
| `results/archive/EXP-2026-09-16-072/REPORT.md` | integration report (this A/B's context) |
| `results/archive/EXP-2026-09-16-072/ab/A{1..5}-decode256.json` | arm A measured runs |
| `results/archive/EXP-2026-09-16-072/ab/B{1..5}-decode256.json` | arm B measured runs |
| `results/archive/EXP-2026-09-16-072/ab/warm-decode256.json` | excluded warm run |
