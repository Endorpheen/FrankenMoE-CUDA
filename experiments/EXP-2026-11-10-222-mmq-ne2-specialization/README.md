# EXP222: pinning `ne2` — 400 → 164 instructions, and −3.7% time

The one concrete item EXP221 produced: `ds` and `scatter` are pinned as constants by four thin
wrappers so the backend folds them, but `ne2` stays a runtime kernel argument, so the two 64-bit
divisions behind `if ne2i == 1` survive in every instantiation. Every Apex MMQ call has `ne2 == 1`.

## The change

One local edit to `device.goose`: replace the `if ne2i == 1 { … } else { i2 = bzi % ne2i;
i3 = bzi / ne2i; }` block with the fast path alone. Valid **only** for `ne2 == 1`.

## Instruction count

| | instructions | `IMAD.HI.U32` |
|---|---:|---:|
| baseline `fg180q_kernel<0>` | 400 | 22 |
| **ne2 pinned** | **164** | **0** |
| native DS4 direct | 157 | 2 |

−236 instructions, −59%, and `IMAD.HI.U32` gone entirely: the two divisions account for 236 of 400
instructions. Against native the emission is now 164 vs 157 — **near parity**, where the honest
comparison before this was 400 vs 157.

`fg180q_kernel<1>` (scatter) is unchanged at 355: it takes the `base = bx * s02` path and never
touches `ne2`.

## Timing — same harness, 2000 launches per arm, separate process per arm

| | goose ns | native ns | gap |
|---|---:|---:|---:|
| baseline | 2054.7, 2102.9 | 1811.9, 1841.8 | +13.40%, +14.18% |
| **ne2 pinned** | 2032.2, 1982.8, 1991.1 | 1812.5, 1772.7, 1810.9 | +12.12%, +11.85%, +9.95% |

goose mean 2078.8 → 2002.0 ns, **−3.69%**. Mean gap against native **13.79% → 11.31%**.

## What this settles

One of my own hypotheses is now resolved with data rather than argument. A 59% cut in instruction
count bought 3.7% of time. So the kernel is **not** instruction-bound — that inference was right but
had no evidence behind it when I first wrote it and then withdrew it. Now it has evidence.

It also means the instruction count was never the explanation for the original slowdown. Removing
236 of 400 instructions leaves 11.3% of gap intact.

## Status

Measured with a variant DSO linked into `/tmp` from the recorded EXP179 link line with only the cuda
object substituted; EXP180's own artefacts untouched. **Not promoted**: the remaining 11.31% is
still above the 2% rule, and this variant is only correct for `ne2 == 1`.

The 3.7% is real and worth promoting once the width is handled properly — the right shape is a fifth
wrapper constant (`ne2_is_one`) selecting between two Goose bodies, so the general case keeps its
divisions and the Apex case does not.

## Byte-exactness of the specialised variant

The EXP180 GPU fixture covers exactly the shapes this variant is valid for: `direct_run` passes
`ne2 = 1`, and the scatter case likewise. Run against the variant DSO, both arms:

    FG180Q_GPU_FIXTURE checks=8 failures=0 arm=goose
    FG180Q_GPU_FIXTURE checks=8 failures=0 arm=native

and the output dumps compare byte for byte:

    files=4 differing=0

So the −3.69% and the 400→164 instruction drop come with no numerical change on `ne2 == 1`.

## Why this is still not promoted

Two reasons, both stated rather than glossed:

1. the remaining gap is **11.31%**, well above the 2% rule;
2. the variant is only correct for `ne2 == 1`. The gate cannot test the general case against it
   because the fixture never exercises `ne2 != 1` — so promoting it as-is would narrow the port's
   correctness envelope without measuring the loss.

## The promotable shape

Add a fifth wrapper constant (`ne2_is_one`) selecting between two Goose bodies, so the general case
keeps its divisions and the Apex case does not. One structural obstacle is known and should be
recorded before anyone starts: `emit_sources.py` asserts that exactly **2** entries are called from
`main` (the ds4 direct/scatter pair), because the extraction mapping is keyed on that. Doubling the
wrappers breaks that assertion, so the extractor has to be taught to carry four entries and the
check must be re-derived from the emission rather than the constant. That is the real work in the
promotable form — the kernel edit itself is four lines.
