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

## The negative control, and it fails as it should

Patching a float's bytes in the file is not a control: both readers see the same change and still agree. The control has
to make the two sides differ, so the check was temporarily given the **wrong key index** for the port's read - an
off-by-one in the comparison only.

```
FG_GGUF_ARRAYS  elements=522/527
llama_model_load: error loading model: llama_model_loader: ported GGUF reader disagrees with the native one (5)
```

Five mismatches - exactly the five elements of the float array - and the load **refuses** to continue. The fault was then
reverted, rebuilt, and the same fixture reports:

```
FG_GGUF_VALUES  integer=7/7
FG_GGUF_ARRAYS  elements=527/527
```

So the branch executes, it detects wrong bytes, and a disagreement is fatal at load. That is what the previous version of
this card could not claim.

## Shipped path

The check runs under `FG_GGUF_DIAG`; nothing changed with the flag unset.
