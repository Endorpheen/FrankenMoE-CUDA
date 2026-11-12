# EXP330: make_qkx3_quants in Goose -- ported, one signed-zero mismatch remains

`make_qkx3_quants` is the per-group scale/min search used by the Q4_K encoder. This experiment ports it and gates it against the
reference, and reports the current result honestly rather than rounding it up.

## Gate

The oracle is the function text extracted **mechanically** from the checked-in source (never retyped), so this is a
transcription check; the artifact-based acceptance remains the end-to-end Q4_K encoder gate.

    600 cases, n = 32, nmax 15/63, weights present or derived, plus constant and outlier vectors
    EXP330_QKX3 checks=39600 failures=55

Every one of the 55 failures is the same class: the returned minimum is `-0` in the reference and `+0` in the port. The scales
and all 32 levels match in every case.

## Why this is worth recording rather than hiding

The reference ends with `*the_min = -min`. When `min` is `+0`, negation gives `-0`, while the port's first form (`0 - min`)
gives `+0`, because IEEE subtraction of equal zeros is `+0`. A rewrite through multiplication (`min * -1`) was expected to
restore the sign but the generated code did not produce `-0`, so the fix is not in place.

Byte-exact equality is the standard used throughout this series, and `-0` and `+0` differ in bytes while comparing equal in
value. So the remaining step is an explicit negation primitive in the bridge (or a bit-level sign flip) rather than an
arithmetic identity, and the gate must stay at zero failures before this helper is considered done.

This is the state at the end of the session slice: port complete, one corner-case class open, next step named.
