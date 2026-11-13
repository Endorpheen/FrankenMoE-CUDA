# EXP321: IQ2_XS decode in Goose, byte-exact

Layout pinned with `offsetof`: `block_iq2_xs` is 74 bytes with `d` at 0, `qs` (uint16[32]) at 2 and `scales[8]` at 66. The
executed `dequantize_row_iq2_xs` is scalar; the grid and sign tables are read from the host through externs.

Structure: eight groups; one scale byte per group whose low nibble drives the first two `l` and the high nibble the last two;
four 16-bit words per group where the low nine bits index `iq2xs_grid` and the high seven index `ksigns_iq2xs`.

This function also corroborates the EXP320 finding from the other side: its own source addresses `qs[4*ib32 + l]` over a
`uint16` array, i.e. an eight-byte stride per group -- exactly the stride the *executed* IQ2_XXS uses, while the checked-in
IQ2_XXS source still shows four. The port follows the executed behaviour.

## Gate

    EXP321_IQ2XS checks=7424 failures=0

Bit-pattern comparison against the real `dequantize_row_iq2_xs`, sizes 256/512/2560/4096, last block fully zeroed.

Mutation: reading the sign index with a shift of 8 instead of 9 produces 3709 divergences out of 7424; reverted before commit.
