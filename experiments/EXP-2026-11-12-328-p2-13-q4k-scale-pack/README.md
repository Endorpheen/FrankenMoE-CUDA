# EXP328: Q4_K scale packing in Goose -- first slice of the encoder direction

The Q4_K encoder is not a one-tick port: `quantize_row_q4_K_impl` depends on `make_qkx3_quants` and `make_qp_quants`, roughly two
hundred lines of iterative float logic, and the gate requires exact equality, so every operation order matters. Rather than start
in the middle of that, this experiment ports the one self-contained piece first: the twelve-byte scale packing (the `j < 4` /
`else` block plus the two high-bit ORs).

## What is ported

Eight 6-bit `Ls` values and eight 6-bit `Lm` values become twelve bytes: 0..3 hold the low scales, 4..7 the low mins, 8..11 carry
`(ls & 0xF) | ((lm & 0xF) << 4)` for `j >= 4` and then receive the high-bit pairs shifted left by six, in the same order the
reference applies its ORs.

## Gate

`tests/fixture.c` contains its own transcription of the decoder's `get_scale_min_k4` read rule, so the check is not the port
reading itself back through its own assumptions. One thousand random cases of eight scale/min pairs:

    EXP328_Q4K_SCALES checks=16000 failures=0

Mutation: shifting the high bits by five instead of six produces 4009 divergences out of 16000; the edit was verified present in
the source first, and reverted to green afterwards.

## Next slices for the encoder

Port `make_qp_quants` next (it is the shorter of the two helpers and is used for both the block scale and the block min), then
`make_qkx3_quants`, then the body of `quantize_row_q4_K_impl`, with the end-to-end gate being: encode with Goose, decode with the
Goose Q4_K decoder from EXP310, and require byte equality with decoding the native encoder's output.
