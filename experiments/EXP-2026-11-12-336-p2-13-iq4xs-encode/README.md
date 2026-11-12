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
