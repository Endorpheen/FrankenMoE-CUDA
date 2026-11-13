# EXP334: TQ1_0 encode in Goose, byte-exact against the exported reference

Fourth encoder with an artifact oracle (`quantize_row_tq1_0_ref` is exported).

## What is ported

Block absolute max as the scale, reciprocal from the unrounded value, stored `d` as its fp16 rounding. Three segments feed the 256
elements: 32 bytes over elements 0..159, 16 bytes over 160..239, and the four `qh` bytes over 240..255. Each byte accumulates five
trits in base 3 (the `qh` byte accumulates four and multiplies once more before packing), then a ceiling division by 243 maps the
value into a byte.

## Gate

    EXP334_TQ1_ENC checks=1566 failures=0

Sizes 256/512/2560/4096 with an all-zero trailing region and denormal inputs, comparing every encoded byte against the reference.

## Mutations, including one that is honestly weak

- Changing the ceiling constant from 242 to 243 produces only **4** divergences out of 1566. It is a real failure, but a weak
  signal, and it is recorded as such rather than presented as strong evidence.
- Replacing the `qh` segment's final multiply by 3 with a multiply by 2 produces **116** divergences, which is the mutation used
  to argue the gate is sensitive.

Both were verified present in the source before counting, and the port was restored to zero failures afterwards.
