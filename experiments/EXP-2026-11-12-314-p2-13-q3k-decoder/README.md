# EXP314: Q3_K decode in Goose, byte-exact -- the K-quant family is now complete

Fifth and last K-quant decoder. Layout pinned with `offsetof` first: `block_q3_K` is 110 bytes with `hmask[32]` at 0, `qs[64]` at
32, `scales[12]` at 96 and `d` at 108. The executed `dequantize_row_q3_K` in `base-quants.o` is scalar, so it is portable under
the EXP190 rule.

Two details are reproduced rather than approximated:

- the twelve scale bytes pack sixteen 6-bit scales, unpacked with the same four little-endian word shuffles the reference uses
  (`kmask1 = 0x03030303`, `kmask2 = 0x0f0f0f0f`), then read as signed bytes and biased by -32;
- the high-bit mask `m` starts at 1 and shifts once per 16-element run, eight runs per block, so it is *not* reset between the two
  halves -- an easy thing to get wrong and invisible until the second half.

## Gate

    EXP314_Q3K checks=7424 failures=0

Bit-pattern comparison against the real `dequantize_row_q3_K`, sizes 256/512/2560/4096, last super-block fully zeroed.

Mutation: shifting `m` by two instead of one produces 2620 divergences out of 7424; reverted before commit.

## Family status

Q2_K, Q3_K, Q4_K, Q5_K and Q6_K all have Goose decoders with byte-exact gates: 7424 checks each, zero failures, and a caught
mutation in every one. What remains of P2-13 is the I-quant family, MXFP4, TQ1_0/TQ2_0 and the repack variants, plus the wiring
into the type-trait table that is waiting behind the base-library blocker.
