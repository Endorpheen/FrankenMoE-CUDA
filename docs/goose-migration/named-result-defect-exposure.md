# Exposure to the confirmed named-result codegen defect (2026-11-10)

The compiler owner reported a defect in `subset-d5e9fdc5233e52b8` and earlier builds: a function
with a **growable array result** that returns early on one path and falls through to a **tail
expression** on another gets the wrong value.

Their reproduction, reproduced here before trusting it:

    fn make(flag:bool)->i64[>..]{
        var first:i64[>..]=[1];
        var last:i64[>..]=[2];
        if flag { return first; }
        last
    }
    make(false)   original expects [2]   actual [1]

Reproduced exactly: `make(false)[0]` returns **1**, not 2. The tail was ignored when selecting the
caller-owned result slot.

## Exposure of the accepted modules

Checked mechanically, not by eye. The defect needs three things together: a `[]`/`[>..]` result
type, a `return` inside the body, and a trailing bare expression. A scan of every `*.goose` under
`experiments/` for functions whose result type contains `[`:

**0 functions match.** No accepted module returns an array type at all.

The loader's `sort_pairs` uses fixed arrays, but as **locals**, and returns `i64`. Its gate is
`FG201L_FIXTURE checks=6342 failures=0`, re-run green on this same build in
`compiler-revalidation-2026-11-10.md`, so the second symptom the owner mentions (a fixed-array
local returned as growable producing invalid C) is not present either.

## Consequence

- EXP199, 202, 203, 209, 210, 213, 215 gate green on this build with **729,479 checks, 0 failures**.
- The accepted runtime `exp215` (2572.56 / 26.47, exact tokens and probabilities) is unaffected.
- The owner's advice — do not accept a newly affected *mixed-return* path until the corrected
  compiler arrives — is honoured: nothing of that shape exists here, and none will be added.

Recorded so the check is repeatable rather than a claim of innocence.
