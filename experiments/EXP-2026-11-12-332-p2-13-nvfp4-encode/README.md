# EXP332: NVFP4 encode in Goose, byte-exact against the exported reference

Second encoder with an **artifact** oracle: `quantize_row_nvfp4_ref` is exported, so the gate compares the whole encoded block
array byte for byte and no source copy is involved.

## What is ported

Per sub-block of sixteen: absolute max, the UE4M3 scale of `amax / 6` (zero when `amax` is not positive, clipped at 448,
subnormal and rounding paths kept), then eight bytes packing two strict-less absolute-error indices from the doubled e2m1 table.
Both UE4M3 conversions remain host primitives.

## Gate

    EXP332_NVFP4_ENC checks=4176 failures=0

Sizes 256/512/2560/4096, an all-zero trailing block and ±1e-30 denormal inputs, comparing every encoded byte against the reference.

Mutation: scaling `amax` by 7 instead of 6 produces 2441 divergences out of 4176; verified present in the source, then restored to
green.

## Note on the fixture

The first version of this fixture wrote past the input buffer (`x[k-32+i]` with a bound of 64) and ASan reported the exact
overflow. That is recorded because it is the second time in this series that a test, not the port, was the thing that was broken.
