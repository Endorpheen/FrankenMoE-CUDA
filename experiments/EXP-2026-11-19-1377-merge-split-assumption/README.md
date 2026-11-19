# EXP-2026-11-19-1377: the merge text reconstructs exactly - 247 587 of 247 587

## The question

EXP-2026-11-19-1376 named two ways to give the ported encoder its merges: keep the loader's string array, or rebuild
the text from `bpe_ranks`, the hash map the vocabulary actually keeps. The second is cheaper and needs no loader - which
matters for a runtime that will not have one - but it assumes every merge is `first + " " + second` with no exception.

## The measurement

The check ran at the parse site, where both the raw string and the two halves exist, on the real vocabulary:

```
FG_MRG_SPLIT_TOTAL checked=247587 violations=0
```

Every merge in Apex reconstructs byte for byte. Nothing was assumed and then relied on: the assumption was tested in the
place where it could fail.

## What it changes

Step 2 of P7-06 no longer needs the loader's merge array. The port can be fed from the vocabulary's own rank map, which
removes a copy of about 2.5 MB and one coupling between the loader and the encoder.

## Why the check is not left in the library

It costs a comparison per merge at load and prints nothing useful once the answer is known; the shipped path stays
clean, as with the probes in EXP-2026-11-19-1364. The check is nine lines and belongs with the serving code when step 2
is written, where it guards the thing it is about.

## What is still missing for step 2

The slot tables (`tok_slots`, `mrg_slots`) and the scratch arrays, sized from the loader's own bounds. The merges were
the input that did not exist; the rest exists and only has to be assembled.
