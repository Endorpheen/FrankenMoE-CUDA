# EXP218: Unicode lowercase mapping in Goose — ported and gated, NOT PROMOTED

`unicode_tolower`: a `std::lower_bound` over `unicode_map_lowercase`, then an equality check,
falling back to the input codepoint. The pre-tokenizer calls it after an apostrophe to match
`(?i:'s|'t|'re|'ve|'m|'ll|'d)`, so it decides whether "don't" stays one token.

Only the **algorithm** moves. The 1433-entry table is project data with an existing owner, so the
bridge hands Goose the table pointer and length; copying it into a Goose array would duplicate data
and force every future edit to be made twice.

## Gate — green

    8328 checks, 0 failures, table=1433
    every key in the table plus both neighbours, 4000 random codepoints, range boundaries,
    and all of ASCII A-Z

Mutations caught: making the search `<=` instead of `<` → 3109 failures; making the final test `>=`
instead of `==` → 1687 failures.

## It is never called by the Apex profile

The witness printed nothing, so I checked the claim instead of assuming it. A diagnostic build
counted entries at the **C++ level**, before any routing decision, so it cannot distinguish "not
called" from "routing not taken" ambiguously:

    FG218DIAG entries=0 goose_calls=0

Verified the diagnostic actually applied, because a silent build failure would make that zero a
lie: the diag `unicode.o` references the counter, the `.so` defines it, and the ordinary EXP218
`unicode.o` does not reference it (control). So: **`unicode_tolower` is not reached by the fixed
32K/128 profile at all.** The prompt has no contractions, and the branch is behind `cpt == '\''`.

## The paired timing, and why it is not a regression

| pair | prefill | decode |
|---|---:|---:|
| 1 | +2.41% | −3.03% |
| 2 | −0.35% | −2.39% |
| mean | +1.01% | −2.71% |

Raw decode: A `[26.01, 26.38]`, B `[25.22, 25.75]`. Decode looks consistently worse and is above the
2% rule, which is exactly the case the rule exists for — so I did not promote.

But the changed code **cannot** be the cause: entry count is zero, so it never runs. A third run
built with a superset of the change (the branch *plus* the counter) measured 25.89, inside the B
range, which is what "the machine drifted between the A and B halves" looks like. I am not
promoting it on the strength of that, and I am not claiming a regression I cannot explain either.
The number and the call count contradict each other, and the call count is a direct observation of
execution rather than an inference from timing, so the call count wins.

## Status

Verified, committed, **not** in the active runtime. `active.json` stays at EXP217. There is no
benefit to promoting it on this profile — it would add a branch to a function this profile never
calls — so nothing is lost by leaving it out until a profile that does reach it.
