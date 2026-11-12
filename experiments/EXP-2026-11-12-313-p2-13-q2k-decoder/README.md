# EXP313: Q2_K decode in Goose, byte-exact

Fourth type of the family, same procedure: pin the layout with `offsetof` first, because this fork reorders blocks.
`block_q2_K` is 84 bytes with `scales[16]` at 0, `qs[64]` at 16, `d` at 80 and `dmin` at 82. The executed
`dequantize_row_q2_K` in `base-quants.o` is scalar, so it is portable under the EXP190 rule.

Algorithm details that matter and are reproduced: two halves of 128, four shifts per half, and two 16-element runs per shift --
the second run reading from `q[l+16]` rather than a second pointer.

## Gate

    EXP313_Q2K checks=7424 failures=0

Bit-pattern comparison against the real `dequantize_row_q2_K`, sizes 256/512/2560/4096, last super-block zeroed.

Mutation: advancing the shift by three instead of two produces 2926 divergences out of 7424; reverted before commit.

## A defect in my own fixture, found and fixed

The fixture inherited from EXP311 zeroed 144 bytes for the degenerate block regardless of the block size. For Q2_K's 84-byte
blocks that overflowed the last block and the fortified `memset` aborted the run -- the gate caught my test, not the port. The
same inheritance meant EXP311's degenerate block was only partially zeroed for its 210-byte blocks; it now zeroes the whole block
and still reports 7424/0, so that claim is unchanged and slightly stronger.
