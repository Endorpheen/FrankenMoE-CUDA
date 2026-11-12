# EXP326: IQ1_M decode in Goose, byte-exact -- every quantized type in the gap list is now ported

Layout pinned with `offsetof`: `block_iq1_m` is 56 bytes with `qs[32]` at 0, `qh[16]` at 32 and `scales[8]` at 48. The executed
`dequantize_row_iq1_m` is scalar; the 2048-entry grid comes from the host through an extern.

Structure: the block scale is itself an fp16 assembled bit by bit from the top nibbles of four 16-bit scale words
(`sc0>>12 | (sc1>>8)&0x00f0 | (sc2>>4)&0x0f00 | sc3&0xf000`). Eight groups of 32 outputs; inside a group `qs[0..3]` give four grid
indices widened by `qh` bits with masks `0x700`, the first two outputs use `dl1` and the last two `dl2`, each of the four has its
own delta sign (bits 3 and 7 of the two `qh` bytes), and the deltas are added to the signed grid value before scaling.

## Gate

    EXP326_IQ1M checks=7424 failures=0

Bit-pattern comparison against the real `dequantize_row_iq1_m`, sizes 256/512/2560/4096, last block fully zeroed.

Mutation: masking the fourth scale word with `0xe000` instead of `0xf000` produces 4352 divergences out of 7424; the edit was
verified present in the source first, and reverted to green afterwards.

## Status of the gap list

With this port, every type named in the P2-13 inventory is covered by a Goose decoder with a byte-exact gate: K-quants, IQ4_NL,
IQ4_XS, MXFP4, TQ1_0, TQ2_0, IQ2_XXS, IQ2_XS, IQ2_S, IQ3_XXS, IQ3_S, IQ1_S and IQ1_M. What remains for P2-13 are the repack
variants (Q4_0_4_4/4_8/8_8, IQ4_NL_4_4/4_8/8_8) and the type-trait wiring that waits behind the base-library blocker.
