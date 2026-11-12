# EXP336: IQ4_XS encode in Goose -- ported, gate red, diagnosis open

This is the multi-block variant of the same implementation IQ4_NL uses, with `super_block_size = 256`, `block_size = 32` and
`ntry = 8`, so the per-block scale search with its -8..8 refinement is live, followed by one fp16 super-scale, six-bit per-block
scales split across `scales_l` and `scales_h`, and a final level pass at the quantised scale.

## Current result, reported as it is

    EXP336_IQ4XS_ENC checks=3944 failures=3854

The first divergence is `byte 4 block 0: ref=f0 ours=0f`, i.e. the two nibbles of `scales_l[0]` are swapped between block 0 and
block 1. That means our per-block quantised scale byte differs for those blocks, not that the packing order is wrong -- the packing
follows the reference's `if (ib%2 == 0) scales_l[ib/2] = l_l; else |= l_l << 4`.

A value of `0xf` in the low nibble corresponds to `l + 32` having low nibble 15, so our `l` for block 0 is one of -17, -1, 15 or
31 where the reference produced one of -32..-16 or 0..14. The super-scale or the clamped `nearest` step is the place to look
first.

## Not yet used as evidence

Nothing here is claimed as working. The port compiles and runs, the gate disagrees, and the first differing byte is recorded so the
next pass starts at the right place instead of re-deriving it.

## Root cause found: the rounding helper was wrong, and it invalidates two earlier experiments

`nearest_int` in this fork is `((bits & 0x007fffff) - 0x00400000)` after adding 12582912.f. The helper used in this series was
`((bits & 0x003fffff) - 0x200000)`, which is not the same operation: for a value of 2.0 the correct form returns 1 and the wrong one
returns -2097151. Fixing it took the IQ4_XS gate from 3854 failures to 126, so the helper was the dominant cause here.

That matters beyond this experiment. EXP329 (`make_qp_quants`) and EXP330 (`make_qkx3_quants`) used the same wrong helper, so their
results are **not trustworthy** and must be re-run after the fix: EXP329's "12400 checks, 0 failures" and EXP330's "55 failures"
were measured with a broken rounding primitive. The parked signed-zero blocker may also look different once the real rounding is
in place.

## Remaining divergence after the fix

    first: byte 2 block 0, scales_h: ref=f3 ours=0c

Decoding the 2-bit fields, the reference has 3,0,3,3,0,3,3,3 and the port has 0,3,0,0,3,0,0,0 -- almost exactly complements, which
means `l0` differs by about 16 for those blocks while `scales_l` now matches. The block scale values or the clamp step are the next
place to look, and the gate stays red until that is resolved.

## Diagnostic export: the block-scale path reads the wrong bytes

A debug entry point (`fg336q_scales_enter`) now exposes the eight per-block scales the port computes. Compared with a transcription
of the same block search on a fixed 256-element input:

    ib=0 ref=0.0256217103 ours=2.35113416e-38   DIFF
    ib=1 ref=-0.0257143658 ours=7.00326282e-30  DIFF
    ... all eight blocks differ

The port's values are on the order of 1e-38 to 1e-30, which is the range you get by reinterpreting small integers as floats. On the
increase, the block-scale path is reading level bytes where it expects floats -- the scratch layout (levels at `scr`, scales at
`scr + 256`) or the offset arithmetic inside `fg336q_scale` is addressing the wrong place.

That also explains the 126 remaining gate failures without invoking the super-scale at all: the scales feeding it were already wrong.
The next step is to verify the scratch addressing directly rather than guessing at the super-scale, which is what the earlier note
suggested and what this measurement replaces.
