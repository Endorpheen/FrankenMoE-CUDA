# EXP-2026-11-19-1401: float arrays are compared, and the fixture now reaches the code that compares them

## The gap this closes

The reader check compared string arrays and integer arrays; an array of `FLOAT32` or `FLOAT64` fell through
(`EXP-2026-11-19-1400` recorded it, and the inventory doc called it the one class not compared). Apex has no float
arrays, so the gap was invisible on the model that matters and would have stayed invisible until another model arrived.

## What was added

- a Goose door `gr_array_raw_bits`, which returns one element's bytes as an integer for any element type of eight bytes
  or fewer - the only way to compare a float array without pretending its bits are an integer of another width;
- a branch in the check for `FLOAT32` and `FLOAT64` arrays comparing those bytes with the native reader's, element by
  element.

## The case that reaches it

Apex has none, so the fixture vocabulary now carries them - a float array `[0.0, -0.0, 1.0, 1.0000001, NaN]` and a float
scalar `-0.0`. The first two and the last are the values a value comparison would let through and a bit comparison
catches: signed zero equals zero by `==` but differs in its sign bit, and NaN equals nothing.

```
FG_GGUF_VALUES  integer=7/7          <- was 6/6, the float scalar joined
FG_GGUF_ARRAYS  elements=527/527     <- was 522/522, the five-element float array joined
```

Both sides read the same bits for all five, including the signed zero and the NaN.

## What is not proven

The comparison is **exercised** but not shown to **fail**. The obvious negative control - patching a float's bytes in
the file - changes what both readers see, so they still agree, which is not a control at all. A real one would inject a
fault into the reader side, and that is not done here. The card states this rather than implying the check was falsified.

## Shipped path

The check runs under `FG_GGUF_DIAG`; nothing changed with the flag unset.
