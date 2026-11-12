# EXP335: IQ4_NL encode in Goose, byte-exact against the exported reference

Fifth encoder with an artifact oracle (`quantize_row_iq4_nl_ref` is exported).

## The path is shorter than the implementation suggests

`quantize_row_iq4_nl_impl` is about a hundred lines, but the reference call uses `super_block_size == block_size == 32` and
`ntry == -1`. That disables the multi-block scale search and the refinement loop, so what actually runs is: weights `x*x`, the
signed maximum at the largest magnitude, `d = max/values[0]`, one level pass with the reference's binary-search index selection,
`d = sumqx/sumq2`, then the pack of two levels per byte. Reading the call before the body saved porting dead branches.

## Gate

    EXP335_IQ4NL_ENC checks=4176 failures=0

Sizes 256/512/2560/4096, comparing every encoded byte against the reference. Mutation: dividing by `values[1]` instead of
`values[0]` in the initial scale produces 3614 divergences out of 4176.

## Two findings worth keeping

1. **A zero or tiny block is not gated, and cannot be.** When `amax < 1e-15` the reference sets the scale to zero and never writes
   `L`, then packs `q4` from that uninitialised array. There is no deterministic oracle for that path, so the fixture uses ordinary
   and extreme-but-normal values and says so instead of pretending coverage.
2. **The first port overflowed the block.** It kept the 32-byte level array inside the 18-byte `block_iq4_nl`, and the harness
   aborted with heap corruption. The levels now live in a separate scratch buffer; the pack writes only the sixteen output bytes.
