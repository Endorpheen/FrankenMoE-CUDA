# EXP319: TQ1_0 decode in Goose, byte-exact -- the TQ family is complete

Layout pinned with `offsetof`: `block_tq1_0` is 54 bytes with `qs[48]` at 0, `qh[4]` at 48 and `d` at 52.

Structure reproduced exactly, because a "clean" rewrite would lose it: three segments feed 256 outputs in order -- `qs[0..31]`
five times (160), `qs[32..47]` five times (80), then `qh[0..3]` four times (16) -- and each product is **truncated to a byte
before** the `x3 >> 8` step, exactly as the reference's `uint8_t` does.

## Gate

    EXP319_TQ1 checks=7424 failures=0

Bit-pattern comparison against the real `dequantize_row_tq1_0`, sizes 256/512/2560/4096, last block fully zeroed.

Mutation: widening the truncation mask from 255 to 1023 produces 4158 divergences out of 7424 -- that is the specific step the
mutation targets, which is why the port keeps the truncation visible instead of folding it into wider arithmetic.

## Status

TQ1_0 and TQ2_0 are both ported and gated. Remaining in P2-13: the IQ2/IQ3 and IQ1 families plus the repack variants, and the
type-trait wiring that waits behind the base-library blocker.
