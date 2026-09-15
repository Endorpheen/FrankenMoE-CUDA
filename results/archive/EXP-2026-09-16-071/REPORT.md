# EXP-2026-09-16-071: can the IQ4_NL down projection be sped up at the MTP verify shape

Status: `DONE` (model-free microbenchmark). No model, no `llama-server`, no profiler was
started. No accepted tree or build was modified. No commit was made.

Question (from the task): the decode down projection runs `MUL_MAT_ID` with IQ4_NL weights at
`ne00=640`, `ne01=2560`, 512 experts, top-10, 3 activation rows (`Ny=3`). EXP-048 attributes
about 20-27 percent of the decode step to this path and records that it was never optimized.
Does a fused/reuse variant (unpack the weight block once, apply it to all `Ny` rows) beat the
stock `ggml_vec_dot_iq4_nl_q8_0` path by at least 1.15x on the full call?

## PRE-REGISTERED BAR (stated before the first run)

Bit-exact outputs (`maxdiff == 0`) AND `>= 1.15x` vs stock at `Ny=3`. Otherwise the conclusion
is that DEC-2/DEC-5 should be closed, and the closing number is the `Ny=3` ratio.

## Verdict

PASS. At the real down shape and `Ny=3` the register-resident reuse variant is **1.620x-1.627x**
faster than the stock library `vec_dot` on the L3-resident working set and **1.621x-1.625x** in
the DRAM-sized working set, with `maxdiff == 0` in every case. The EXP-021 buffer variant also
passes at `Ny=3` (1.176x-1.177x). DEC-2/DEC-5 are therefore **not** closed by this measurement;
the number that would have closed them (the `Ny=3` ratio) is 1.62x, above 1.15x.

This is a kernel-level result only. It does not claim any model-level or end-to-end decode
effect; see Caveats.

## Setup

- Machine: AMD Ryzen 9 5950X, single pinned core `taskset -c 8`, 32 MiB L3 per CCD, 64 MiB total.
- Compiler: `gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0`, flags `-O3 -march=native -std=gnu11`.
- Stock kernel: `ggml_vec_dot_iq4_nl_q8_0` linked from the accepted build
  `build/exp062-ring3/bin/libggml-cpu.so.0.22.0`
  (`sha256 c80ad356daae49e4515c2c54b70f366bb611a8b6b1f50e4e3591f1b9c7825d6a`), tree
  `work/llama.cpp-exp062-ring3` at its patched working state.
- Harness: adapted from `benchmarks/exp021_fused_bench.c`
  (`sha256 d91cc9997d9eb47e250524d870d12658072f6c8e542ea8335b7a51f0a9c68f1f`).
- Activation rows are quantized to Q8_0 once and shared by every variant; the fp32 activation
  block scales are precomputed once per token.
- Each row/token set is timed as `NREP=5` measured repetitions plus one warmup; the median is
  reported. Every variant processes bit-identical weight bytes.

The stock path is faithful to the real code: `ggml_compute_forward_mul_mat_id_one_chunk`
(`ggml/src/ggml-cpu/ggml-cpu.c:1520`) calls the opaque `vec_dot` function pointer once per
(row, token) with `nrc=1`. The only existing fused `mul_mat_id` path is for IQ2_S
(`ggml-cpu.c:1936`); IQ4_NL has none.

### Variants

- `stock` - library `ggml_vec_dot_iq4_nl_q8_0`, one call per (row, token), through the type
  function pointer.
- `inline` - control: the same AVX2 arithmetic inlined, still one call per (row, token), no
  reuse. Separates the opaque-call cost from the reuse gain.
- `fusB` - EXP-021 fused variant: unpack the row once into a per-row buffer, then apply it to
  every token (store/load through the buffer).
- `fusR` - register-resident reuse: unpack each 32-block once and apply it to every token in the
  same iteration, with per-token accumulator registers. This is the literal "unpack once, apply
  to all Ny rows" variant and the strongest ALU-side candidate.

The dequant/apply arithmetic is copied from the AVX2 `ggml_vec_dot_iq4_nl_q8_0`
(`arch/x86/quants.c:3920`), split at the weight-only / per-token boundary. Per-token
accumulation order is identical to stock, which is why all variants are bit-exact.

An important methodological note: the first harness build converted fp16 scales through the
`ggml_fp16_to_fp32` function, which is a PLT call in the hot loop; the library uses the
`ggml_table_f32_f16` lookup. That penalty applied once per weight block to `fusB`/`fusR` and to
`inline`, while stock used the table. After switching the harness to the same table lookup the
`Ny=3` `fusR` ratio moved from ~1.18x to ~1.62x. EXP-021's fused numbers carried the same
penalty, so its low-`Ny` conclusion is partly an artifact of the harness, not only of the
algorithm.

## Measured table (run 1, `exp071-down-iq4nl.txt`)

One call = one expert's down projection: 2560 rows x `Ny` tokens. Weight bytes per call =
921600. Times are ns/call, median of 5. `x` ratios are stock/variant.

### Resident, working set 0.88 MiB (fits L3)

| Ny | stock ns | inline ns | fusB ns | fusR ns | inl x | fusB x | fusR x | maxdiff | verdict |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|:--|
| 1 | 79940.6 | 72614.6 | 95487.0 | 79670.6 | 1.101 | 0.837 | 1.003 | 0.0e+00 | FAIL |
| 2 | 158368.2 | 143638.1 | 149337.4 | 112680.5 | 1.103 | 1.060 | 1.405 | 0.0e+00 | PASS |
| 3 | 238401.4 | 214582.5 | 202715.0 | 146535.8 | 1.111 | 1.176 | **1.627** | 0.0e+00 | **PASS** |
| 4 | 318220.7 | 287549.8 | 260728.9 | 192134.8 | 1.107 | 1.221 | 1.656 | 0.0e+00 | PASS |
| 8 | 633288.7 | 570747.6 | 472005.2 | 351172.6 | 1.110 | 1.342 | 1.803 | 0.0e+00 | PASS |

Pure reuse gain (`fusR` vs the inlined no-reuse control) at `Ny=3`: **1.464x**.

### Streaming, working set 112 MiB (above L3)

Single-core read-sum ceiling on the same buffer: 22.79 GiB/s = 24.474 bytes/ns.

| Ny | stock ns | inline ns | fusR ns | inl x | fusR x | stock B/ns | fusR B/ns | maxdiff | verdict |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|:--|
| 1 | 836344.2 | 752610.3 | 825074.3 | 1.111 | 1.014 | 11.02 | 11.17 | 0.0e+00 | FAIL |
| 2 | 1605288.0 | 1441917.2 | 1150835.0 | 1.113 | 1.395 | 5.74 | 8.01 | 0.0e+00 | PASS |
| 3 | 2378182.5 | 2141127.2 | 1466897.8 | 1.111 | **1.621** | 3.88 | 6.28 | 0.0e+00 | **PASS** |
| 4 | 3169144.5 | 2853122.0 | 1902041.1 | 1.111 | 1.666 | 2.91 | 4.85 | 0.0e+00 | PASS |
| 8 | 6383359.5 | 5723082.6 | 3545028.1 | 1.115 | 1.801 | 1.44 | 2.60 | 0.0e+00 | PASS |

Pure reuse gain at `Ny=3`: **1.460x**.

## Reproducibility (run 2, `exp071-down-iq4nl-run2.txt`)

Independent second run of the same binary:

| Case | Ny=3 fusR x | Ny=2 fusR x | Ny=1 fusR x |
|---|---:|---:|---:|
| resident | 1.620 | 1.421 | 1.015 |
| streaming | 1.625 | 1.497 | 0.980 |

`maxdiff == 0` for every variant in both runs. Run-to-run spread at `Ny=3` is under 0.5 percent.

## Where the crossover is

`fusR` has no loss region at this shape: at `Ny=1` it is parity (0.98x-1.02x), and it wins for
every `Ny >= 2` (1.40x-1.50x at `Ny=2`). The crossover from "not worse" to "clearly better" is
therefore between `Ny=1` and `Ny=2`. The EXP-021 buffer variant `fusB` crosses between `Ny=1`
(0.84x) and `Ny=2` (1.06x).

The task anticipated a loss at `Ny=3` based on EXP-021. That does not reproduce here: the two
reasons are (a) the EXP-021 numbers were measured with the buffer variant, and (b) the harness's
per-block `ggml_fp16_to_fp32` call penalized every fused variant. With the library's table
lookup and register residency, the reuse benefit dominates already at `Ny=2`.

## Bytes/ns and the DRAM ceiling

The read-sum ceiling of one pinned core is 21.6-22.8 GiB/s (23.2-24.5 bytes/ns). At `Ny=3`, stock
delivers only 3.88 bytes/ns of unique weight traffic and `fusR` 6.28 bytes/ns, i.e. 16 percent
and 26 percent of the ceiling. The L3-resident and DRAM-sized tables produce the same per-expert
time (242.7 us vs 244.2 us per expert for stock at `Ny=3`), so the kernel is not bandwidth-bound:
it is ALU/issue-bound. That is why an ALU-side reuse change is the correct lever here, and why
the result is stable across cache regimes.

Sanity cross-check: stock at `Ny=3` is 1.55 ns per 32-element block, consistent with the
EXP-019 measurement of the same library kernel (1.22-1.66 ns/block across regimes).

## Caveats

1. Scope is the kernel work of one full down `MUL_MAT_ID` call. The real call additionally pays
   the per-expert dispatch, gather and scheduling; those are not removed by the fused kernel and
   would dilute the end-to-end gain. The 1.62x is a kernel-work ratio, not a full-call wall-time
   ratio.
2. Reuse requires at least two tokens to share one expert. With 3 verify tokens x top-10 and 512
   experts, an expert selected by only one token gives no reuse; `fusR` is then at parity (0.98x-
   1.02x), not a loss. The realized gain depends on the actual per-expert group-size distribution
   at decode. EXP-048 reports near-full 2-3-row sharing for this projection, which is exactly the
   region measured here.
3. The streaming table reuses a rotating window of 128 experts; this makes the working set
   113 MiB (above L3) but the kernel is compute-bound, so it does not change the conclusion.
4. No model was run. This microbenchmark neither proves nor bounds any decode throughput change.

## Files created (none changed)

All under `results/archive/EXP-2026-09-16-071/`:

| File | sha256 |
|---|---|
| `exp071_down_iq4nl_bench.c` | `86ae59cce7e84c6978dcabd81523718fda542486507b196d3cf2a6ab756ea647` |
| `exp071_down_iq4nl_bench` | `21eae5ed4537f0ac9ea298c6beb2422186d2db437c6cd107f76fc9b2c1751f58` |
| `exp071-down-iq4nl.txt` | `11f3c15f5000de57adb1843b19b3143d4d40a9b6e3ab7be6837c3aa5f601eadc` |
| `exp071-down-iq4nl-run2.txt` | `55e229dcb312ef5f6e8e065161ae115a654441892d9d5b620bf6b2d247e8e2d8` |
| `exp071-stderr.txt` | `610aec6dfadc659ab967b637be0fe2a958827d65748f46c7e1957bc14f3b507b` |
| `exp071-stderr-run2.txt` | `610aec6dfadc659ab967b637be0fe2a958827d65748f46c7e1957bc14f3b507b` |
| `REPORT.md` | this file |

No tracked file in the repository and no file of `work/llama.cpp-exp062-ring3` or
`build/exp062-ring3` was modified. No commit was made.

## Exact commands

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
