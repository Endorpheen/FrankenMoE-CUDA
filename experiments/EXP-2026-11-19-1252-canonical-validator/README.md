# EXP-2026-11-19-1252: the canonical validator checked against the real one — and my duplicate removed

## What happened first, and it was a mistake worth recording

I ported the validation a second time, into `goose/misc/config-validate.goose`, and registered it. The tree
already had **`goose/cli/config-validate.goose`** (P9-01 S1) doing exactly this: the same decision chain with
rule numbers instead of English, 48 rule constants, called `cfg_validate(i[], f[])`.

Two things followed:

- the duplicate was **deleted** rather than merged: rule 26 of this project says a duplicated definition is debt
  with no purpose, and here it would also have failed the link the moment both were composed;
- the differential was pointed at the **existing** module, which is what made the harness worth writing: it now
  checks the tree's own port instead of my copy of it.

That my grep did not find the existing module is the same class of mistake as reading the wrong build file: I
searched for `validate` in `goose/` with a filter that skipped `cli/`, and concluded "not ported".

## The result on the canonical module

```
canonical validate: cases=4000 fails=0 uncovered=0
```

4000 random `RunConfig`s, the real `bmoe::validate` linked from `libbmoe_core.a`, the canonical
`cfg_validate(i[32], f[5])` loaded from the built library, and the native message mapped back to its rule index.
Zero failures and zero configurations the native rejected outside the rules the module implements.

44 rules are exercised, including the nested MoE block, the speculative family and the route_ahead exclusions.

## Why this is the strongest form available here

- the native side is compiled and called, not transcribed;
- the module under test is the one in the tree, not one I wrote for the harness;
- the harness is a link against `bmoe_core` and a `dlopen` of the built library, so it runs in seconds with no
  model and no GPU;
- it compares on the decision (which rule, in order) rather than on wording.

## Reproduce

```
bash experiments/EXP-2026-11-19-1252-canonical-validator/build.sh
```
