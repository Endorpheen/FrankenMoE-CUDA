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

## Update: the sign is lost upstream, not at the final negation

An explicit negation primitive was added (`fg330q_neg`, which returns `-v` and preserves `-0`), the generated C was verified to
call it in all three places, and the gate still reports the same 55 failures. So the final `-min` is not where the sign goes
wrong: our `min` itself already has the opposite zero sign from the reference's.

That moves the fault to `this_min = (sum_l2*sum_x - sum_l*sum_xl)/D` and to the assignments that follow it. The values compare
equal, and the only difference is which zero they are, so the remaining work is to reproduce the exact arithmetic of that
expression (including the `this_min > 0` clamp writing a positive zero) rather than to patch the last line again.

The end-to-end encoder gate will show whether this matters in practice: a `-0` versus `+0` dmin produces identical decoded floats
unless the scale multiplies a signed value, so it may be invisible there. It is recorded now because byte equality is the standard
used for every other gate in this series.

## Instrumentation attempt (incomplete, recorded for the next pass)

A tracing hook (`fg330q_trace`) was added to both the Goose source and the bridge to print `this_min` at the end of each `is`
iteration and `min` before the final negation, then removed again because it produced **no output** under `FG_TRACE=1` while the
fixture still reported the same 55 failures. `nm` on the bridge object showed no trace symbol, so the object or the binary was not
the one the run used -- the hook was unverified, and an unverified diagnostic is not evidence.

Next pass must, before reading anything into the trace: verify `nm` shows the hook in the object, verify the generated C calls it,
and only then compare the two sequences. The tree is left without the hook so the next attempt starts from a clean state.
