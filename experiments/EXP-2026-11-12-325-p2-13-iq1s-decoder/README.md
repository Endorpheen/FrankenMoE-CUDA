# EXP325: IQ1_S decode in Goose, byte-exact

Layout pinned with `offsetof`: `block_iq1_s` is 50 bytes with `d` at 0, `qs[32]` at 2 and `qh[8]` (uint16) at 34. The executed
`dequantize_row_iq1_s` is scalar; the 2048-entry grid comes from the host through an extern.

Structure: eight groups of 32 outputs. One `qh` word per group supplies the grid's top three bits for each `l`, a 3-bit scale in
bits 12..14, and the delta sign in bit 15. Two details distinguish this format and are reproduced rather than generalised:

- grid values are **signed** bytes;
- the delta (0.125) is added to the integer grid value **before** scaling, not to the scaled result.

## Gate

    EXP325_IQ1S checks=7424 failures=0

Bit-pattern comparison against the real `dequantize_row_iq1_s`, sizes 256/512/2560/4096, last block fully zeroed.

Mutation: reading the delta sign from bit 12 instead of bit 15 produces 3456 divergences out of 7424. The edit was verified
present in the source before the number was quoted, and reverted to green afterwards.
