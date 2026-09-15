# EXP-2026-09-16-072: integrate the register-resident fused IQ4_NL down path

Status: `DONE` (integration + model-free gate). No model and no `llama-server` process was
started. No accepted tree or build was modified. No commit was made.

Deliverable of the task: create `work/llama.cpp-exp072-iq4nl-fused` as a copy of the accepted
`work/llama.cpp-exp062-ring3`, add the model-free-confirmed register-resident fused IQ4_NL
`MUL_MAT_ID` down path for the verify case (same decode-only window as the accepted EXP-046
IQ2_S path), build `build/exp072-iq4nl-fused` with the same flags, and run the model-free gate.
The measured kernel this integrates is `fusR` from EXP-071 (1.627x vs stock
`ggml_vec_dot_iq4_nl_q8_0` at K=640, rows=2560, Ny=3, bit-exact).

## Verdict

Implemented, built, and gated. The fused path is byte-exact against the stock path on the real
down shape through the real `ggml_mul_mat_id` op (0 differing floats of 51200 at rows=2 and of
76800 at rows=3, and 0/307200 differing bytes in the paired real-call bench). It activates on
the opened verify window (fused-group and fused-dispatch counters advance) and is disabled by
`GGML_CPU_IQ4NL_FUSED=0` (same binary, counters stay at 0). On the real call, the measured
speed ratio depends on the per-expert group-size distribution: 1.3114x at Ny=3 with full
sharing at 1 thread, 1.1485x at 12 threads, 1.0519x on a routing mix close to the measured
EXP-046 histogram, and parity (1.0131x) on a sparse routing. This is a candidate build; it is
not the default runtime and no model-level gain is claimed.

## What was integrated

The tree is a byte-identical copy of `work/llama.cpp-exp062-ring3` (verified with `diff -rq`,
empty; the copy also carries the same `.git`). The integration mirrors the accepted EXP-046
IQ2_S structure at every level:

1. `ggml/src/ggml-cpu/ggml-cpu.c`
   - env switch `GGML_CPU_IQ4NL_FUSED`: on by default, `"0"` disables, read once into a static
     atomic (same convention as `GGML_CPU_IQ2S_FUSED`).
   - env switch `GGML_CPU_IQ4NL_FUSED_STATS`: counters off by default, any other value on.
   - `g_iq4nl_fused_window` atomic plus `ggml_cpu_iq4nl_fused_set_window(bool)`; the window is
     closed by default and never inferred from the batch size.
   - 15 diagnostic counters (`groups[4]`, `fused_groups[4]`, `tokens[4]`, calls, window_opens,
     fused_dispatches) with `ggml_cpu_iq4nl_fused_stats_get/dump`, dumped at exit when stats are
     on.
   - `ggml_compute_forward_mul_mat_id_iq4nl_one_chunk_e72()`: per 16-row chunk, each IQ4_NL
     weight block pair is unpacked once (LUT expand, exactly as the stock AVX2 kernel) and then
     applied to all 2-3 activation rows, each row keeping its own `acc1`/`acc2` registers. The
     per-row arithmetic order is identical to `ggml_vec_dot_iq4_nl_q8_0` (AVX2): one fmadd per
     block pair per accumulator, final `acc1+acc2` then the same `hsum_float_8` reduction tree.
   - fused branch in `ggml_compute_forward_mul_mat_id`; guard:
     `mode==1 && window && type==IQ4_NL && ids->ne[1] in [2,3] && cne1 in [2,3] &&
     ne00 % 64 == 0 && !ggml_is_numa()`. The even-block guard is explicit: the stock scalar tail
     for an odd last block is not reproduced, so odd shapes stay on the stock path. The chunk
     scheduling (16-row chunks, atomic chunk stealing, shared `atomic_current_chunk`) is the
     same as the IQ2_S path.
2. `ggml/include/ggml-cpu.h`: public declarations for the IQ4_NL window and stats functions
   (new comments in English; the pre-existing Russian comments of the EXP-046 block are
   untouched).
3. `tools/server/server-context.cpp`: the IQ4_NL window is opened and closed at the same
   decode site as the IQ2_S window, with the same `is_prompt` predicate - every row of the
   batch view must be a decode-phase row (sampled + draft); prefill chunk views keep both
   windows closed.
4. `tests/CMakeLists.txt` plus two new model-free tests (see the gate).

`kvalues_iq4nl_e72` is a local 16-byte copy of `kvalues_iq4nl` from `ggml-common.h` (the tables
are only emitted for translation units that define `GGML_COMMON_IMPL_C`, and `ggml-cpu.c`
includes `ggml-common.h` in declaration mode only). A mismatch would break the byte-exact gate
against the stock vec_dot, so the duplication is self-checking.

## Build

Exact configure command (same flags as `build/exp062-ring3`; its `CMakeCache.txt` external
entries are identical to the new build after excluding the build paths, `diff` empty):

```bash
cmake -S work/llama.cpp-exp072-iq4nl-fused -B build/exp072-iq4nl-fused \
  -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DGGML_CUDA=ON \
  -DCMAKE_CUDA_ARCHITECTURES=89 -DGGML_OPENMP=ON -DLLAMA_OPENSSL=ON -DLLAMA_BUILD_SERVER=ON
cmake --build build/exp072-iq4nl-fused -j 10 --target test-iq4nl-fused-mmid test-iq4nl-fused-mmid-bench
cmake --build build/exp072-iq4nl-fused -j 10 --target llama-server
cmake --build build/exp072-iq4nl-fused -j 10 --target test-backend-ops test-iq2s-fused-mmid
```

All targets built clean (one warning class from `ggml-backend-meta.cpp`, pre-existing; no error).
Binaries (sha256, full list in `sha256-binaries.txt`):

| Binary | sha256 |
|---|---|
| `build/exp072-iq4nl-fused/bin/llama-server` | `38b17c7f6832e554f7176e7d86869172c9fd77f8e001892f1b1590299466d4fd` |
| `build/exp072-iq4nl-fused/bin/libggml-cpu.so.0.22.0` | `646755eeb69903795a4b0c5b677d643807b5c9a4867ecda017abf7e22589d033` |
| `build/exp072-iq4nl-fused/bin/test-iq4nl-fused-mmid` | `ec7d5bb7f4d07e93c8a271a2ff0cdc647fb7363fed3b5719581a649bed1bc288` |
| `build/exp072-iq4nl-fused/bin/test-iq4nl-fused-mmid-bench` | `302fa2c5412fc00c22f089e6a37c76cd077e0eb432f45f40ad5e96603d2ed62a` |
| `build/exp072-iq4nl-fused/bin/test-backend-ops` | `ec5dd019abbf9549e4cabed2fd60b70903d4578a0211670e5ef43c93d92750af` |

Protected-tree check: `build/exp062-ring3/bin/libggml-cpu.so.0.22.0` still hashes
`c80ad356daae49e4515c2c54b70f366bb611a8b6b1f50e4e3591f1b9c7825d6a`, the value recorded in
EXP-071; `work/llama.cpp-exp062-ring3` was not written (its only state changes are the copy
baseline it already had).

## Model-free gate

### 1. Correctness gate on the real shapes (`test-iq4nl-fused-mmid`)

`tests/test-iq4nl-fused-mmid.cpp` builds one real `ggml_mul_mat_id` graph with the down shape
(`src0` IQ4_NL 640 x 2560 x 512, `src1` f32, `ids` i32 10 x rows), runs it with the window
closed and open, and compares the full output bytes. Cases: A rows=1 (must not fuse); B rows=2
with a shared expert 0 and edge ids 0/511; C rows=3 with expert 7 shared by all three, edges
0/511; D IQ2_S weights in the open IQ4_NL window (type guard); E rows=2 shared but odd block
count `nb=3` (shape guard); F rows=4 shared expert (checkpoint-tail shape, must not fuse). Each
case also runs at 1 thread and compares against the baseline hash.

Enabled arm (default), `./bin/test-iq4nl-fused-mmid`:

```
ALL PASS
  caseB: 0/51200 differing floats
  caseC: 0/76800 differing floats
IQ4NL_FUSED_STATS groups[c1,c2,c3,c4+]=247,8,3,2 fused_groups=0,4,2,0 tokens[c1,c2,c3,c4+]=10,95,81,74 iq4nl_groups_seen=260 window_opens=8 fused_dispatches=39
```

Disabled arm (`GGML_CPU_IQ4NL_FUSED=0`), same binary:

```
ALL PASS
IQ4NL_FUSED_STATS groups[c1,c2,c3,c4+]=247,8,3,2 fused_groups=0,0,0,0 tokens[c1,c2,c3,c4+]=10,95,81,74 iq4nl_groups_seen=260 window_opens=8 fused_dispatches=0
```

The group/token histograms are identical in both arms (the graph did the same work); only the
fused counters differ. `fused_dispatches=0` with the env set to `"0"` and hash equality in
every case is the disable evidence.

### 2. Real-call paired bench (`test-iq4nl-fused-mmid-bench`)

One graph compute = one full `MUL_MAT_ID` call over 512 experts. Arms alternate OFF (window
closed) and ON (window open), 10 warmup + 100 measured per arm, 3 pairs; the timer wraps
`ggml_backend_graph_compute`. The OFF and ON outputs are compared over the full dst bytes once
per run. `fused_dispatches_delta` proves the arm. Cases use hand-built routing sets:
`2s` 10 groups of 2; `3s` 10 groups of 3; `3m` 2x3+3x2+18x1 (40% of row-work in groups >= 2,
close to the 45.5% derived from the recorded EXP-046 histogram: c1 73.5%, c2 18.1%, c3 8.25%);
`3r` 1x3+1x2+25x1 (sparse).

| Case | threads | OFF median (us) | ON median (us) | ratios (3 pairs) | median | exactness | dispatch delta OFF/ON |
|---|---:|---:|---:|---|---:|---|---|
| 3s | 12 | 240.4-269.8 | 209.6-209.9 | 1.2868 / 1.1485 / 1.1449 | 1.1485x | 0/307200 | 0 / 13200 |
| 2s | 12 | 168.5-194.3 | 152.6-155.1 | 1.2738 / 1.1279 / 1.0962 | 1.1279x | 0/204800 | 0 / 13200 |
| 3m | 12 | 240.7-264.0 | 228.8-230.2 | 1.1468 / 1.0519 / 1.0514 | 1.0519x | 0/307200 | 0 / 6600 |
| 3r | 12 | 230.3-295.8 | 230.1-231.6 | 1.2792 / 1.0131 / 0.9946 | 1.0131x | 0/307200 | 0 / 2640 |
| 3s | 1 (pinned) | 2312.1-2327.8 | 1766.6-1768.5 | 1.3114 / 1.3164 / 1.3074 | 1.3114x | 0/307200 | 0 / 1100 |
| 3s, `GGML_CPU_IQ4NL_FUSED=0` | 12 | 241.5-270.7 | 241.3-242.9 | 1.1197 / 0.9977 / 1.0006 | 1.0006x | 0/307200 | 0 / 0 |

The dispatch delta per measured arm equals `threads x fused_groups x 110 calls` (e.g. 12 x 10 x
110 = 13200), which is exactly the expected count. The first pair of every run has an inflated
OFF median; this cold-start artifact also appears in the disabled control (1.1197x there), so
the median of the three pairs is quoted and the pair values are all listed.

Interpretation: in the real call the kernel saving is diluted by the per-call fixed work (q8
conversion, 512-group scan, chunk scheduling) and by groups with `cne1 == 1` that stay on the
stock path. At 1 thread and full sharing (3s, every row-work in fused groups) the whole call is
1.3114x. With 12 threads the same call is 1.1485x. On a realistic routing mix (3m) the full
call is 1.0519x; on sparse routing (3r) it is parity (1.0131x), consistent with the EXP-071
finding that a group of one loses nothing.

### 3. `test-backend-ops` MUL_MAT_ID

`./bin/test-backend-ops -o MUL_MAT_ID -b CPU`:

```
872/872 tests passed
Backend CPU: OK
2/2 backends passed
```

CUDA0 was skipped by the filter. The suite includes `MUL_MAT_ID(type_a=iq4_nl,...)` cases
(n=1 and n=32); they run with the window closed, so they exercise the stock path plus the new
guards.

### 4. EXP-071 harness cross-check against the new build

The EXP-071 harness source (sha256 `86ae59cc...`) was recompiled against
`build/exp072-iq4nl-fused` (binary in this directory,
`exp071_down_iq4nl_bench_exp072`, sha256 `9bed83eb...`) and run pinned to core 8. The stock
library vec_dot is unchanged (same lib source; `libggml-cpu` rebuilt), and the harness's
embedded `fusR` reproduces the confirmed result:

| Case | EXP-071 | new build |
|---|---:|---:|
| resident Ny=3 fusR vs stock | 1.627x | 1.590x |
| streaming Ny=3 fusR vs stock | 1.621x | 1.628x |

`maxdiff == 0` for every variant and Ny in both tables. The integrated kernel is the same
algorithm; the small resident-table difference is run noise (EXP-071 run 2 gave 1.620x on the
same binary that first gave 1.627x).

### 5. Regression: accepted IQ2_S path

`GGML_CPU_IQ2S_FUSED=1 ./bin/test-iq2s-fused-mmid` in the new build: `ALL PASS`, with
`IQ2S_FUSED_STATS ... fused_groups=0,5,2,0 ... fused_dispatches=51`, so the EXP-046 path still
fuses and stays byte-exact.

## Exact diff

The complete diff between `work/llama.cpp-exp062-ring3` and `work/llama.cpp-exp072-iq4nl-fused`
(excluding `.git`, generated with `diff -ruN`) is the artifact `exp072-integration.diff`
(sha256 `2a694fa8f318d81ff9a6bd392c9a1dc1804ef3f8583806c0ddf85014dcc52c96`, 850 lines, only
additions, no deletions and no changes to existing lines):

| File | +/- |
|---|---|
| `ggml/src/ggml-cpu/ggml-cpu.c` | +228 -0 |
| `tests/test-iq4nl-fused-mmid-bench.cpp` (new) | +268 -0 |
| `tests/test-iq4nl-fused-mmid.cpp` (new) | +251 -0 |
| `ggml/include/ggml-cpu.h` | +11 -0 |
| `tests/CMakeLists.txt` | +7 -0 |
| `tools/server/server-context.cpp` | +3 -0 |

## Files created or changed

Changed/added inside `work/llama.cpp-exp072-iq4nl-fused` (sha256 in `sha256-sources.txt`):

| File | sha256 |
|---|---|
| `ggml/include/ggml-cpu.h` | `5e504589fb21bda5922c0256e7e941c5cdbcbf78bb313fc763e5d581b9fe4379` |
| `ggml/src/ggml-cpu/ggml-cpu.c` | `9971cc34a2d39f00e991221646765f72ac56de206872972651f7204210cfe4f1` |
| `tools/server/server-context.cpp` | `6b5a3cccc72047e6fccb00315b285bb6cfa66b134f2548ff21dc4bb751b06dde` |
| `tests/CMakeLists.txt` | `acfa80f18194f43a8a43d0cf0a9d0d775c289a52236862b9b016a4de35c72b76` |
| `tests/test-iq4nl-fused-mmid.cpp` (new) | `16a0a256731425f46a38593478065d8841cdbdcfcad44041ab77f9791550b6ff` |
| `tests/test-iq4nl-fused-mmid-bench.cpp` (new) | `016d31eba91156d6ca8de6d10192f21315f4cd3ee25066577f83de029c885461` |

Created in this repository: `build/exp072-iq4nl-fused/` (build tree) and
`results/archive/EXP-2026-09-16-072/` (this report, the diff, the raw logs, the harness binary
and its outputs, the sha256 lists). All artifact hashes are in `sha256-artifacts.txt`.

Nothing else was written: `work/llama.cpp-exp062-ring3`, `build/exp062-ring3`, every other
build directory and `work/llama.cpp-integration` are untouched.

## Caveats

1. This is a candidate build in an isolated tree, not a default runtime and not a model-level
   result. No model or server was started, per the task.
2. The window is opened only by the server decode path; the model-free tests open and close it
   explicitly, which is why activation can be shown without a model. The server-side opening
   condition is reviewed in the diff but was not exercised end to end.
3. The real-call ratio is routing-dependent. Cases `2s`/`3s` are the upper bound of sharing;
   `3m` is the closest to the recorded EXP-046 group histogram; `3r` is the lower bound. The
   single-thread `3s` number (1.3114x) is the cleanest measurement of the full call with all
   row-work fused.
4. The fused kernel is compiled for `__AVX2__ && __FMA__` only; other targets keep `false` in
   the guard and fall back to the stock path, as in EXP-046.
5. The odd-block shape guard means the fused path covers only even `ne00/32`; the real down
   shape (K=640, nb=20) is covered.
