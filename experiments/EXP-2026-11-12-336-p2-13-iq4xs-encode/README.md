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

## The diagnostic itself is mis-wired, and that is the lesson of this pass

The export reported above was wired to a generated symbol chosen by its argument shape. All three indices tried produced the same
garbage, which means either the call was never changed or the symbol is a different helper -- and the values being level bytes
reinterpreted as floats fits the *block* helper being called with the output buffer where it expects the level buffer.

Two earlier passes in this session failed the same way: an `LD_PRELOAD` hook that could not bind because of `-Bsymbolic`, and a
trace hook that produced no output because the object under test was not the one running. The common cause is guessing which
generated symbol or object is live instead of proving it.

The rule for the next pass, recorded so it is not re-learned: identify the generated function by following the call graph (which
function calls `fg336q_scale`, and which is called from `main`), and verify the object by a symbol or a marker before drawing any
conclusion from its output.

Nothing in this experiment is claimed working: the gate is red at 126 failures and the diagnostic is not trustworthy yet.


## Corrected diagnostic: the function was never generated, and the block scales are right

Following the call graph instead of guessing indices settled it. `fg336q_scales` was **absent from the generated C** because `main`
never called it, so every index tried earlier was some other helper -- which is exactly why the output looked like level bytes
reinterpreted as floats. Making `main` reference the function produced it as `fg336q_fn_18`, identified by the fact that it calls
`fg336q_fn_5` (the block search) and is called from `main`.

With that wiring the diagnostic says the block search is **correct**:

    ib=0..7  ref == ours, bit for bit (0.0256217103, -0.0257143658, ...)

So the 126 remaining gate failures are not in the per-block scale search at all. The fault is in the super-scale step: `dsup`,
`l0`, or the `scales_l`/`scales_h` packing. That is a much smaller surface than the earlier notes implied, and it is measured rather
than guessed.

## Block scales, dsup and l0 all match on a fixed input

The export was extended to `l0` and the super-scale, and the same harness compares them:

    dsup ref=0.00080357393 ours=0.00080357393
    ib=0..7  l0 ref == ours (31, -32, -30, -31, 31, -31, 31, 31)

So the per-block search, the super-scale and the clamped `nearest` are all correct on this input. The remaining gate failures therefore
sit in the **second level pass** (re-levelling at the quantised scale with `idl`) or in the quant-byte packing -- not in anything
measured so far.

The next pass must reproduce an actual *failing* gate input instead of a synthetic one, because every component checks out on the
synthetic case. That is the difference between this failure and the previous ones: the surface is now three steps, two of which are
proven correct.

## Correction: the l0 comparison was circular, and the real divergence is in the block scale

Diagnostics on an actual failing gate input show it immediately:

    block 0 bytes  ref: 4c 20 f3 fc 1d dc f0 ee
    block 0 bytes  our: 52 a0 0c 03 e3 34 0f 12

Bytes 0-1 are `d`. The reference stored `0x204c` (positive, about 0.00424) and the port stored `0xa052` (negative, about 0.00844), so
the super-scale itself differs -- roughly a factor of two with the opposite sign.

That invalidates the previous "dsup and l0 verified" claim as stated: the harness computed the reference `l0` **from the port's own
scale array**, so both sides shared the port's numbers and could only agree. The check was circular, and the honest statement is
that `l0` is consistent with the port's scales, not that it matches the reference.

What is now established: the divergence is upstream of `l0`, in the per-block `d` values for this input -- either the block search
itself on this data, or the `max_scale`/`amax_scale` selection that produces `d = -max_scale/32`.

The next step is a transcription compared against the *native* block bytes on this exact input, not against the port's own arrays.

## The first failing case is not the first size

A harness that reproduces the fixture's input generation for the 256-element case shows the port matching the transcription exactly:

    ib=0..7  ref == ours (0.26519233, -0.259101689, -0.230306298, ...)
    native d bytes 3e a0  (0xa03e, about -0.0083, matching dsup from those scales)

Yet the fixture's first failure reported `ref=4c 20 ours=52 a0`, a different `d` altogether. The only consistent explanation is that the
first failing case is **not** the 256-element one: the fixture consumes the RNG across four sizes, so the block that fails first belongs
to a later case whose input the harness above did not reproduce.

So the earlier conclusion "the divergence is in the block scale for this input" is only true for *that* input; on the 256-element case the
block scales and the super-scale agree with the reference. The next step is to reproduce the exact case the fixture fails on (consume
the RNG in the same order and size) before comparing anything further -- otherwise the comparison is against the wrong data, which is
the third time in this experiment that a measurement was taken on inputs other than the failing one.
