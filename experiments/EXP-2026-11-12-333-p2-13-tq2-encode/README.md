# EXP333: TQ2_0 encode in Goose, byte-exact against the exported reference

Third encoder with an artifact oracle (`quantize_row_tq2_0_ref` is exported).

## What is ported

Block absolute max as the scale, its reciprocal taken from the **unrounded** value while the stored `d` is the fp16 rounding of
the same number -- using the rounded value for the reciprocal would be the obvious mistake and is avoided. Then two 32-byte groups,
each covering four 32-element slices, with every value rounded, biased by +1 (so -1, 0, 1 map to 0, 1, 2) and shifted by two bits
per slice.

The fp32-to-fp16 conversion is a host primitive written to round to nearest even; the reference's macro does the same.

## Gate

    EXP333_TQ2_ENC checks=1914 failures=0

Sizes 256/512/2560/4096 with an all-zero trailing region and denormal inputs, comparing every encoded byte against the reference.

Mutation: removing the +1 bias produces 1856 divergences out of 1914; verified present in the source, restored to green.
