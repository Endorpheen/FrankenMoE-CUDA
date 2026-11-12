# EXP322: IQ3_XXS decode in Goose, byte-exact

Layout pinned with `offsetof`: `block_iq3_xxs` is 98 bytes with `d` at 0 and `qs[96]` at 2. The executed
`dequantize_row_iq3_xxs` is scalar; grid and sign tables come from the host through externs.

Structure: the first 64 `qs` bytes carry two 8-bit grid codes per `l` (eight bytes per group, advanced by 8 each group); the last
32 bytes are `scales_and_signs`, four per group, whose top nibble scales and whose remaining 28 bits give four 7-bit sign indices.
The group factor here is **0.5**, unlike IQ2_XXS's 0.25 -- a detail worth stating because the two functions otherwise look alike.

## Gate

    EXP322_IQ3XXS checks=7424 failures=0

Bit-pattern comparison against the real `dequantize_row_iq3_xxs`, sizes 256/512/2560/4096, last block fully zeroed.

Mutation: changing the group factor from 0.5 to 0.25 produces 7424 of 7424 divergences; reverted before commit.
