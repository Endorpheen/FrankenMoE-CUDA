# EXP310: Q4_K decode in Goose, byte-exact against the native reference

First slice of P2-13's gap list, chosen by the EXP190 rule: the executed `dequantize_row_q4_K` in `base-quants.o` contains zero
`%xmm`/`%ymm`/`%zmm` instructions, so a faithful port is possible, and Q4_K is on the Apex path (Q4_K_M model).

## What is Goosed

The whole algorithm: the per-super-block `d`/`dmin` read, the 6-bit scale/min unpacking (`get_scale_min_k4`, both the `j < 4` and
the packed branches), the four 64-output groups with low nibbles first and high nibbles second, and the `d1*q - m1` arithmetic.
Externs are only byte loads, one half-to-float conversion and the float store.

## Gate

`tests/fixture.c` compares every output float by bit pattern against the real `dequantize_row_q4_K` from `libggml-base`, over
four sizes (256, 512, 2560, 4096 elements), with the last super-block of each case zeroed as a degenerate block:

    EXP310_Q4K checks=7424 failures=0

Mutation check: adding one to the second group's scale makes the fixture fail with 3025 of 7424 checks diverging, so the gate is
sensitive rather than vacuous. The mutation was reverted before this commit.

## Not yet live

The decoder is not wired into the type-trait table yet, so there is no live witness and no claim of one. That is the next step
for this family, together with the same treatment for Q6_K, which is the other type the Q4_K_M model uses.
