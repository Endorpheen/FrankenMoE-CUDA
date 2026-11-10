# EXP221: MMQ Q8_1 codegen retested on the newest compiler — NO CHANGE

EXP180 ported the MMQ Q8_1 quantizer byte-exactly but was 11.4% slower, and the recorded cause was
the emission itself: 824 SASS instructions against native's 160, mostly address arithmetic. Since
then the compiler shipped nonlocal-return and named-result fixes. This retest asks one question —
**did the emission change?** — with a reproducible measurement rather than an impression.

Verdict: **no. Identical instruction counts.** MMQ remains blocked.

## Measurement

Both sides go through the same path: emit `device.goose` → extract the device functions
(`emit_sources.py`'s mechanical extraction) → `nvcc -O3 -arch=sm_89 -cubin` → `cuobjdump -sass` →
count instruction lines.

| emission | total | IMAD | BRA |
|---|---:|---:|---:|
| recorded (EXP180 baseline) | 824 | 238 | 29 |
| **subset-7ef08c2d (newest)** | **824** | **238** | **29** |

Native reference from EXP180: 160 instructions.

Byte size of the extracted header did grow: 33529 → 36506 bytes (+8.9%), and the raw generated C
grew 1808 → 2144 lines with more `%`, `*` and shifts. **C size and SASS instructions disagree**, and
the SASS number is the one that matters — the extra C is not becoming extra instructions. Worth
knowing before anyone reads growth in the emission as a regression.

My first attempt counted IMAD as 238 where EXP180's note says 206. I did not reconcile the two:
likely a different match (all `IMAD` mnemonics vs a counted subset). The comparison that matters is
like-for-like between the two emissions, and that is exact.

## The baseline is a file, not a build

The pinned compiler `subset-84f31960f9f3031e` **cannot compile today's `device.goose`**:

    device.goose:188: error: expected identifier
        if (iqs&(15 as i64))==0&&iqs<96{

The parser fix that made this source reproducible landed after that build. So the 824-instruction
baseline is the extraction already recorded in `source/device-functions-generated.cuh`, not a fresh
run of the old pin. That is the artifact which produced the measured 11.4% slowdown, so it is the
right baseline, but it means the "old" side cannot be regenerated from source on this machine.

## What would unblock MMQ

The instructions are address arithmetic, not the quantizer's arithmetic. Native computes all
addresses once per block; the emission recomputes them per access. A fix on the compiler side, or a
rewrite that passes precomputed byte offsets in so the kernel body is `base + i0`, is what the
number says is needed. Nothing measured here shows any of that has happened.

## Reproduce

    /tmp/sass180.py   # copies cuda-runtime.cu beside each generated header, then nvcc + cuobjdump

Kept as a script rather than prose so the 824 figure can be re-derived after any compiler release
instead of being quoted.

---

# Correction: the 824-vs-160 figure compares the wrong scopes

Before accepting "the emission is 5× worse", I counted per function instead of per cubin. It does
not hold up.

## Per-kernel counts, same flags, same arch, same tool

| side | kernel | instructions |
|---|---|---:|
| Goose | `fg180q_kernel<0>` (direct) | **432** |
| Goose | `fg180q_kernel<1>` (scatter) | **392** |
| native | `quantize_mmq_q8_1<layout 2, false>` (DS4 direct) | **168** |
| native | `quantize_mmq_q8_1<layout 2, true>` (DS4 scatter) | **264** |

The old figure paired the **sum of both Goose kernels** (824) against **one** native instantiation
(160). Two separate scope errors:

1. 824 is two kernels, not one. Per kernel it is 432 and 392.
2. 160 was not even the DS4 instantiation — DS4 is layout 2, which is 168 direct and 264 scatter.
   The 160 belongs to layout 1.

Corrected gap: **432 vs 168 on direct (2.6×) and 392 vs 264 on scatter (1.5×)** — not 5.15×.

## And 432 is not a coincidence

    168 + 264 = 432

`fg180q_kernel<0>` is exactly as large as **both** native DS4 variants combined. The kernel comment
already states why: "the Goose body keeps the reductions, the layout branches and both the direct
and the scatter block index." Native emits those as separate template instantiations; the Goose body
carries both paths in each instantiation, so part of the count is code that never runs there.

So the emission is close to native in size **per unit of functionality**. What it is not, is
specialized.

## What this changes

- The instruction count was never the explanation for the 11.4% slowdown. That measurement
  (2007 ns vs 1801 ns) stands on its own and is what blocks the promotion.
- A 2.6× count producing only 11.4% more time means the kernel is **not** instruction-bound. Register
  pressure, occupancy or the extra per-element branch is the more likely lever, and instruction
  count is the wrong thing to optimise.
- The concrete next step is no longer "wait for the compiler": it is to emit the scatter and layout
  paths as separate Goose variants so each instantiation matches one native kernel. That is runtime
  work, not compiler work.

I have not verified which of the two paths the `scatter` argument selects in each wrapper — both
call sites pass `0` as the last argument, which does not obviously match the two-path structure, and
I stopped rather than assert a cause. That is the first thing to check when this resumes.

---

# Second correction: the specialization claim was wrong

I wrote above that "the Goose body carries the direct and scatter block index in every
instantiation". **That is false.** Checked the source instead of the kernel comment:

    fn fg180q_mmq_ds4_direct (...)  { fg180q_mmq(..., 1 as i32, 0 as i32, ...); }
    fn fg180q_mmq_ds4_scatter(...)  { fg180q_mmq(..., 1 as i32, 1 as i32, ...); }

`ds` and `scatter` are **constants in four thin wrappers**, exactly as the source comment says, so
the backend folds the body the way the native template does. The last argument at the call site
(`0`) is the `base` ABI argument, not `scatter` — that is what confused me. Specialization is
present and was never the problem.

## So 432 is live code, and the count difference is real

| | instructions | registers | LDL/STL | MOV |
|---|---:|---:|---:|---:|
| Goose `fg180q_kernel<0>` | 432 | 30 | 0 | 68 |
| Goose `fg180q_kernel<1>` | 392 | 34 | 0 | — |
| native DS4 direct | 168 | 26 | 0 | — |
| native DS4 scatter | 264 | 38 | 0 | — |

2.6× (direct) and 1.5× (scatter) is a genuine difference in emitted code. 68 `MOV` of 432 is 16%,
which is what unoptimised register allocation looks like.

## And my inference about the cause was also speculation

I wrote "a 2.6× count producing only 11.4% more time means the kernel is not instruction-bound", and
proposed register pressure or occupancy. **The resource numbers do not support that either**: no
spills on either side (LDL/STL = 0), 30/34 registers against native's 26/38. Occupancy is not
degraded.

So the honest state is:

- the emission is 2.6× larger — measured;
- it does not spill and does not use more registers — measured;
- the port is 11.4% slower — measured;
- **why** is not established.

## Third time I proposed a cause before verifying it

First the 5× figure (wrong scope), then "both paths in every instantiation" (wrong), then "not
instruction-bound, it must be occupancy" (unsupported). Each took one command to check and each was
written as a conclusion first. The pattern is the thing to fix, not any single claim: for a
performance question, report the measurement and say "cause not established" until a controlled
differential points at one.

## What to do next, now that the wrong explanations are out

Compare like for like on the actual work rather than on code size: run the two kernels under `nsys`
in one Apex pass and read achieved occupancy, memory throughput and stall reasons. Static counts
have now produced three wrong conclusions; a profile of the running kernel has not been tried.

---

# Operator-level timing reproduced; profiling blocked by permissions

The end-to-end Apex delta is noisy, so I revived EXP180's own kernel harness (`tests/timing.cu`),
which launches the real entry point 2000 times on a fixed Apex-like shape and reports ns per launch.
Each arm is a separate process because the toggle is read per call.

    goose  2054.7 ns   (min 2017.9  max 2089.0)
    native 1811.9 ns   (min 1803.7  max 1820.7)
    goose  2102.9 ns   (min 2068.1  max 2223.1)
    native 1841.8 ns   (min 1827.8  max 1872.8)

**+13.5% and +14.2%**, consistent with EXP180's +11.4% from the full Apex delta. This is a cleaner
measurement of the same effect: same process shape, fixed input, 2000 launches, alternating arms,
spread under 1% within each arm.

Getting the harness to run needed `LD_LIBRARY_PATH=runtime/exp180/run` — without it the binary
resolved `libggml-cuda` from exp180 but `libggml-base` from elsewhere and died on
`undefined symbol: fg160_fusion_enabled`, which *is* defined in that directory's libggml-base. The
stale binary is not broken; the search path was.

## The microarchitectural question cannot be answered here

    ==ERROR== ERR_NVGPUCTRPERM - The user does not have permission to access
              NVIDIA GPU Performance Counters on the target device 0.

`ncu` cannot read counters. Unblocking needs either root or
`NVreg_RestrictProfilingToAdminUsers=0` on the nvidia module — a system change, and system settings
are off limits for this task. `nsys` would work but only gives durations, which the harness above
already provides more precisely.

So the cause of the 13.5% remains **not established**, and now for a stated environmental reason
rather than for lack of trying. Three wrong hypotheses came from static metrics; the metric that
would settle it is the one this machine will not report.

## Exact command for when permission exists

    export LD_LIBRARY_PATH=<repo>/runtime/exp180/run:$LD_LIBRARY_PATH
    ncu --metrics \
      gpu__time_duration.sum,\
      sm__warps_active.avg.pct_of_peak_sustained_active,\
      dram__throughput.avg.pct_of_peak_sustained_elapsed,\
      smsp__inst_executed.sum,\
      smsp__average_warps_issue_stalled_long_scoreboard_per_issue_active.ratio \
      -c 4 --launch-skip 100 <repo>/runtime/exp180/build/timing

Run once with `GGML_GOOSE_CUDA_MMQ_Q8_1=1` and once with `=0`. The harness is already linked at
`runtime/exp180/build/timing`; nothing else needs rebuilding.

---

# Third compiler release tested: still 432 / 392

`subset-78968fac6ca99ab2` (sha256 `78968fac6ca99ab2…b8b95bb`) emits the same kernel:

| emission | `fg180q_kernel<0>` | `<1>` | total |
|---|---:|---:|---:|
| EXP180 baseline (recorded artifact) | 432 | 392 | 824 |
| `subset-7ef08c2d0b65dffe` | 432 | 392 | 824 |
| **`subset-78968fac6ca99ab2`** | **432** | **392** | **824** |

The extracted header is byte-identical in size (36506) across both new builds. Native DS4 is 168
(direct) and 264 (scatter).

Three qualified releases, no movement. The recent compiler work — nonlocal return, named results,
`bytes_of` — is orthogonal to address-arithmetic lowering, so this is the expected result rather
than a surprise, but it is now measured rather than assumed.

Reproduce with `measure_sass.py` + `extract_device.py` in this directory; point the extraction at a
fresh emission of `device.goose` from any build.
