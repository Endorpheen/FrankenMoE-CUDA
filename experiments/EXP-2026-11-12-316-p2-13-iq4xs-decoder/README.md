# EXP316: IQ4_XS decode in Goose, byte-exact

Layout pinned with `offsetof`: `block_iq4_xs` is 136 bytes with `d` at 0, `scales_h` at 2, `scales_l[4]` at 4 and `qs[128]` at 8.
`QK_K` is 256 in eight groups of 32. The executed `dequantize_row_iq4_xs` is scalar, so it is portable under the EXP190 rule.

Details reproduced: the per-group 6-bit scale `ls = low4 | (high2 << 4)` where the low part comes from `scales_l[ib/2]` shifted by
`4*(ib%2)` and the high part from `scales_h >> 2*ib`, then `- 32`; and within each 16-byte group the low nibbles are outputs 0..15
and the high nibbles 16..31, with the 16-entry value table embedded rather than referenced.

## Gate

    EXP316_IQ4XS checks=7424 failures=0

Bit-pattern comparison against the real `dequantize_row_iq4_xs`, sizes 256/512/2560/4096, last block fully zeroed. The fixture
fills every byte of a block except `d`, so the scale fields are exercised rather than left at zero.

Mutation: changing the bias from `ls - 32` to `ls - 31` produces 7424 of 7424 divergences; reverted before commit.
