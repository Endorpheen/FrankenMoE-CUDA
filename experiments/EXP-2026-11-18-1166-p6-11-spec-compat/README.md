# EXP-2026-11-18-1166: P6-11 slice 1, draft-model compatibility

`common_speculative_are_compatible` decides from vocabulary scalars only: vocab type
equality, `add_bos` agreement plus the bos id when the flag is set, the same for eos, and a
bounded vocabulary-size difference (`SPEC_VOCAB_MAX_SIZE_DIFFERENCE`).

`src/compat.goose` transcribes the decision order. The gate is a table of ten cases, each
isolating one condition:

```
spec compat: cases=10 fails=0
```

The first run reported one mismatch, and it was my table rather than the predicate: the
case named "no add_eos" had set the target's add_eos flag to 1 while the draft's was 0, so
the native verdict is correctly "incompatible" and my expected value was wrong. Corrected.

Limits, stated: this gate is a table derived from the native decision order, not a call
into a native instance with two real vocabularies. The live comparison belongs to the MTP
acceptance step, where two models are loaded anyway.

## Slice 2: prefix agreement

`g_spec_prefix` returns the length of the agreeing prefix between the drafted tokens and
the tokens the target produced, and writes the first disagreement position, which is the
rebase point the native `accept()` uses.

```
spec prefix: cases=7 fails=0
```

Seven cases: full agreement, disagreement at each of the three positions, `k=0`, `k=1`, and
a window shorter than the arrays.

Scope note carried from the scope file: in this tree acceptance is target verification, so
this predicate is the whole decision - there is no probability-ratio test to port, and the
predicate consumes no RNG, which keeps the fixed-seed guarantee from P6-10 intact.
