# EXP318: TQ2_0 decode in Goose, byte-exact

Layout pinned with `offsetof`: `block_tq2_0` is 66 bytes with `qs[64]` at 0 and `d` at 64; `QK_K` is 256. The executed
`dequantize_row_tq2_0` is scalar, so it is portable under the EXP190 rule.

Structure reproduced: two 32-byte chunks, four 2-bit passes per chunk, value `(q - 1) * d`. The output order is chunk, then
pass, then byte -- not interleaved.

## Gate

    EXP318_TQ2 checks=7424 failures=0

Bit-pattern comparison against the real `dequantize_row_tq2_0`, sizes 256/512/2560/4096, last block fully zeroed. The fixture
randomises the 64 quant bytes and writes a valid half for `d`, so the scale participates in every comparison.

Mutation: changing `(q - 1)` to `(q - 2)` produces 7424 of 7424 divergences; reverted before commit.
