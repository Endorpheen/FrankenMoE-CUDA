# EXP320: IQ2_XXS decode in Goose -- and a source/artifact divergence found by the gate

Layout pinned with `offsetof`: `block_iq2_xxs` is 66 bytes with `d` at 0 and `qs[64]` at 2; `QK_K` is 256 in eight groups of
32. The grid and sign tables (`iq2xxs_grid`, `ksigns_iq2xs`, `kmask_iq2xs`) are read from the host through externs, so they are
not duplicated and cannot drift.

## The gate caught a divergence between the source and the executed library

The checked-in `ggml-quants.c` reads each group at `x[i].qs + 4*ib32` for `2*sizeof(uint32_t)`. A C replica written from that
source disagreed with the real `dequantize_row_iq2_xxs` on 215 of 256 values, and the first mismatch was exactly at the start of
the second group. Switching the replica to an eight-byte stride made it agree on all 256.

So the executed `base-quants.o` advances eight bytes per group while the checked-in source says four. This is the same class of
drift as the base-library reproducibility problem under P6-02, and it is recorded here because a port written from the source
would be subtly wrong and would still "run". The Goose module uses the executed stride, with the reason in a comment.

## Gate

    EXP320_IQ2XXS checks=7424 failures=0

Bit-pattern comparison against the real `dequantize_row_iq2_xxs`, sizes 256/512/2560/4096, last block fully zeroed.

Mutation: changing the group factor from 0.25 to 0.5 produces 7424 of 7424 divergences; reverted before commit.
