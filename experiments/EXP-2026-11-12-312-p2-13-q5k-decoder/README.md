# EXP312: Q5_K decode in Goose, byte-exact, third type of the family

Layout pinned with `offsetof` before writing anything, because EXP311 showed this fork reorders blocks:
`block_q5_K` is 176 bytes with `d` 0, `dmin` 2, `scales[12]` 4, `qh[32]` 16, `qs[128]` 48.

The executed `dequantize_row_q5_K` in `base-quants.o` is scalar (zero `%ymm`/`%zmm`), so it is portable under the EXP190 rule.

## Gate

    EXP312_Q5K checks=7424 failures=0

Every output float compared by bit pattern against the real `dequantize_row_q5_K` from `libggml-base`, sizes 256/512/2560/4096,
last super-block zeroed.

Mutation: shifting the fifth-bit mask by one instead of two per group gives 1202 divergences out of 7424; reverted before commit.

## Wiring is deliberately deferred

Installing these decoders in the type-trait table means rebuilding and relinking `libggml-base`, which is exactly the library
currently under the P6-02 blocker (its deployed bytes cannot be reproduced from source). Relinking it now would replace the
artifact the Apex baseline is measured against and would muddy that diagnosis, so the wiring waits for P6-02 while the pure
decoder work continues.
