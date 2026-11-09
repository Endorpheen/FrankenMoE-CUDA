# EXP186 — REJECTED: Q6_K codegen gap, not promoted

Hypothesis: `mul_mat_vec_q<Q6_K>` can be transferred to Goose with the same structure
that worked for Q4_K in EXP185, and enabled by default.

## What works

The transfer is **functionally correct**. 8 checks over 4 cases per arm, all four
outputs byte-identical to native (ncols 256, 1024, 2048 two-channel, 4096),
compute-sanitizer memcheck 0 errors, and the model witness
`FG186Q_CUDA_MMVQ_Q6K paths=0x1` appears in both Goose runs. Apex content sha256 is
identical to the reference in all four runs (`e68df05b`), so the model produces exactly
the same text.

## Why it is rejected anyway

Paired Apex, same DSO, only this op switched, order native → goose → goose → native:

| arm | prefill tok/s | decode tok/s |
|---|---:|---:|
| native-rollback a | 2561.06 | 26.52 |
| goose a | 2582.11 | 25.95 |
| goose b | 2579.59 | 25.87 |
| native-rollback b | 2603.62 | 27.26 |
| native median | 2582.34 | 26.89 |
| goose median | 2580.85 | 25.91 |
| delta | −0.06% | **−3.66%** |

Prefill is flat but decode regresses 3.66%, well past the 2% threshold. Host load was
comparable across the four runs (median 2.76–3.13), so this is not a load artefact.

## Measured cause

The kernel was profiled in isolation with `nsys`, not guessed at:

| arm | kernel | median per launch |
|---|---|---:|
| goose | `fg186q_kernel` | 21.82 us |
| native | `mul_mat_vec_q<Q6_K,1,false,false,false>` | 17.41 us |

That is **+25% per launch**, which is the right order of magnitude to explain a 3.66%
decode loss on a kernel that dominates the per-token path.

SASS instruction counts for the same two kernels: **536 vs 208**. The instruction mix
is dominated by address arithmetic — 42 `IMAD.MOV.U32`, 41 `IADD3`, 35 `IMAD`, 25
`IMAD.WIDE.U32`, 20 `IMAD.HI.U32`, 21 `IMAD.X` — because every Goose read recomputes a
full byte address from scratch instead of letting nvcc fold it into the load's
immediate offset. A second difference: native's loads are `LDG.E.CONSTANT` (the
read-only path, from `__restrict__` parameters) while Goose's are plain `LDG.E`.

This is the same code-generation gap that blocked EXP180 (MMQ Q8_1, 824 vs 160
instructions) and it needs a compiler-side answer, not another kernel rewrite.

## Attempted and measured, no effect

- Moving the iqs decomposition from `i64` to `i32` so the divides fold to shifts:
  instruction count stayed at 536 and the byte comparison still passed 4/4. Reverted
  nothing, kept it, but it did not close the gap.

## State left behind

`active.json` is untouched: the qualified runtime stays at EXP185. The EXP186 module
builds and is byte-exact, so it can be re-measured the moment the codegen gap is
solved; `GGML_GOOSE_CUDA_MMVQ_Q6K=0` is not needed because nothing enables it.

## What would unblock it

1. A compiler change that emits a base-plus-immediate address per access instead of
   recomputing the full offset (this is the EXP180 blocker as well).
2. Or hand-shaping the Goose source so every address is precomputed once in the host
   module and the kernel only does `base + i0` — worth one more attempt now that the
   instruction budget is understood, but it was already tried six times for EXP180.

## Follow-up diagnosis: where the extra instructions actually come from

The instruction counts were re-examined per opcode on the two kernels rather than in
aggregate, and the profile points at one specific cause.

| opcode | goose | native |
|---|---:|---:|
| total | 504 | 193 |
| IMAD | 172 | 45 |
| IADD3 | 58 | 14 |
| ISETP + SEL | **79** | **7** |
| LDG (16-bit) | 11 | 11 |

The loads match exactly, so the difference is not memory access. It is the predicate and
select traffic: 79 against 7.

That traffic comes from the Goose runtime's checked arithmetic helpers. Every `gg_div`,
`gg_mod`, `gg_udiv` and `gg_umod` in the emitted body is wrapped as

    if (!b) fail("division by zero"); if (a==INT64_MIN && b==-1) fail("division overflow"); return a/b;

so each one compiles to a zero test, an overflow test and a trap branch. The Q6_K body
contains 13 such calls.

Part of that is provably redundant: 5 of the 13 have a **constant** divisor (4 or 16, from
`QI6_K` and `QK8_1` shifts), so the compiler could drop both guards, but it cannot see
through the helper's interface. The other 8 have variable divisors and their guards are
semantically necessary.

5 of 13 explains only part of the 72-instruction excess, so this is a contributor rather
than the whole story; the remaining IMAD and IADD3 gap is still the pointer arithmetic
that nvcc folds into load immediates in the native version and the Goose emitter does not.

Both halves of the cause are outside what a kernel rewrite can fix from here: dropping
the guards needs either a compiler mode that knows a divisor is non-zero, or a Goose
integer type that cannot be zero. Recorded so the next attempt starts from the profile
rather than from the instruction total.
