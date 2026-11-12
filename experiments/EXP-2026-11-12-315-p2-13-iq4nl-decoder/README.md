# EXP315: IQ4_NL decode in Goose, byte-exact -- first of the I-quant family

Layout pinned with `offsetof`: `block_iq4_nl` is 18 bytes with `d` at 0 and `qs[16]` at 2; `QK4_NL` is 32. The executed
`dequantize_row_iq4_nl` in `base-quants.o` is scalar, so it is portable under the EXP190 rule.

One ordering detail that differs from the K-quants and is reproduced rather than assumed: the two nibble halves are **not**
interleaved. The sixteen low nibbles fill outputs 0..15 and the sixteen high nibbles fill 16..31. The 16-entry value table is
embedded from `ggml-common.h` (`kvalues_iq4nl`), so the module has no table pointer to keep in sync.

## Gate

    EXP315_IQ4NL checks=7424 failures=0

Bit-pattern comparison against the real `dequantize_row_iq4_nl`, sizes 256/512/2560/4096 (all multiples of 32), last block fully
zeroed as the degenerate case.

Mutation: changing table entry 9 from 13 to 14 produces 459 divergences out of 7424; reverted before commit.
