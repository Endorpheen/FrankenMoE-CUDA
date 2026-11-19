# EXP-2026-11-19-1259: the acceptance profile after the P9-01 stage

The library changed twice since the last acceptance run: `llama-arch.cpp` now calls the recipe registry
(89 comparisons per model load) and the canonical validator is exported. Neither should alter behaviour, and
both are confirmed rather than assumed:

```
decode [44.76, 45.71, 45.84]   prefill [1955.7, 2261.5, 2279.9]   sha {'c2ff42cc1219'}
main runtime ok: 1192 global symbols, 0 duplicates, 148 extern
```

The hash is the same control hash as every accepted run of this session, the speed range matches the previous
stage (decode 44.76-45.84 against 44.93-46.29), and the accumulated unit still typechecks with no duplicate
symbols.

## What this is and is not

It is a **parity check**: an inert addition must not move the output or the speed, and this says it did not.

It is not a new performance claim: one run of three clients, no paired comparison against the pre-stage build, so
the honest statement is "no change is visible at this resolution", not "zero cost".

## Why it is worth recording separately

The two changes were made for verification reasons, not to improve anything, and that is exactly the kind of
change that quietly costs something. Confirming parity for them is cheaper than discovering later that an
"inert" hook was on a hot path - which happened earlier in this session with the per-merge comparator, where the
hook cost a call per merge until it was put behind a flag.
