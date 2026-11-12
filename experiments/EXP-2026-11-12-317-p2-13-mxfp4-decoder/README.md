# EXP317: MXFP4 decode in Goose, byte-exact

Layout pinned with `offsetof`: `block_mxfp4` is 17 bytes with `e` at 0 and `qs[16]` at 1; `QK_MXFP4` is 32. The executed
`dequantize_row_mxfp4` is scalar, so it is portable under the EXP190 rule.

Two pieces are reproduced exactly and are easy to get subtly wrong:

- the scale is an E8M0 exponent through `ggml_e8m0_to_fp32_half`: for `e < 2` the bits are the denormal patterns
  `0x00200000 << e`, otherwise the exponent is `(e - 1)` in the top bits. Both branches are in the pool;
- the values are the *doubled* e2m1 table `kvalues_fp4` (0,1,2,3,4,6,8,12,0,-1,-2,-3,-4,-6,-8,-12), embedded rather than
  referenced, because the "doubled" convention is what pairs with the halved scale.

## Gate

    EXP317_MXFP4 checks=7424 failures=0

Bit-pattern comparison against the real `dequantize_row_mxfp4`, sizes 256/512/2560/4096 (multiples of 32), last block fully
zeroed. The fixture randomises `e` across the whole byte range, so the denormal branch is exercised too.

Mutation: changing table entry 5 from 6 to 7 produces 463 divergences out of 7424; reverted before commit.
