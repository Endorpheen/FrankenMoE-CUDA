# EXP324: IQ3_S decode in Goose, byte-exact

Layout pinned with `offsetof`: `block_iq3_s` is 110 bytes with `d` 0, `qs[64]` 2, `qh[8]` 66, `signs[32]` 74 and `scales[4]` 106.
The executed `dequantize_row_iq3_s` is scalar; the grid comes from the host through an extern.

Structure: four iterations of 64 outputs. Each iteration reads one scale byte (low nibble for the first half, high for the
second), two `qh` bytes (one per half) that widen the nine-bit grid codes by a single bit with a per-`l` shift of `8-2*l` or
`7-2*l`, eight code bytes and eight sign bytes; each `l` produces four outputs from one grid and four from the next.

## Why there is a C replica in this experiment

The first Goose attempt failed 7273 of 7424 checks. Rather than guess, a C replica of the same reading was compiled against the
real `libggml-base` and matched it on all 256 values -- which located the fault in the *port*, not in the reading. The bug was
a missing `l * 32` byte offset when writing each `l`'s eight outputs. With that fixed:

    EXP324_IQ3S checks=7424 failures=0

Bit-pattern comparison against the real `dequantize_row_iq3_s`, sizes 256/512/2560/4096, last block fully zeroed.

Mutation: changing the first-half scale from `1 + 2*low` to `1 + low` produces 3424 divergences out of 7424. The source-level
edit was verified present before the result was quoted; reverted to green afterwards.
