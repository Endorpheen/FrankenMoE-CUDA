# EXP327: NVFP4 decode in Goose -- the last missing decoder type

Layout pinned with `offsetof`: `block_nvfp4` is 36 bytes with `d[4]` (u8) at 0 and `qs[32]` at 4. `QK_NVFP4` is 64 in four
sub-blocks of 16, each with its own UE4M3 scale byte and eight quant bytes whose low nibbles are the first eight values and high
nibbles the last eight. Values come from the same doubled e2m1 table as MXFP4.

The scale conversion is a host primitive (`ggml_ue4m3_to_fp32`): zero for `x == 0` or `0x7F`, otherwise a 4-bit exponent with
bias 7 and 3 mantissa bits, halved to pair with the doubled table. Keeping it as an extern matches how MXFP4 treats the E8M0
conversion, and the two conversions stay visibly different instead of being unified by accident.

## Gate

    EXP327_NVFP4 checks=7424 failures=0

Bit-pattern comparison against the real `dequantize_row_nvfp4`, sizes 256/512/2560/4096 (multiples of 64), last block fully
zeroed. The fixture randomises every byte including the scale bytes, so both UE4M3 branches are in play.

Mutation: taking the high nibble as `b >> 3` instead of `b >> 4` produces 3253 divergences out of 7424; the edit was verified
present in the source first, and reverted to green afterwards.

## Coverage statement

With this port every type that the executed `libggml-base` reports as having a `to_float` now has a Goose decoder: the sixteen
portable block types in this series plus the scalar and simple block types already covered through EXP143's CPY path. The
remaining work in P2-13 is the encoder direction and the type-trait wiring.
