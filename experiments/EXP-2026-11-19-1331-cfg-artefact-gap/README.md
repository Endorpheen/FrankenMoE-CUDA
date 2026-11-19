# EXP-2026-11-19-1331: config validation cannot be pointed at the shipped library by relinking, and the reason is not mechanical

## What was tried

Config validation is the one composed module without a shipped-artefact check (EXP-2026-11-19-1330). The obvious move
is to relink its gate against the library, as EXP-601's writer harness was. The blocker was assumed to be its
`gg_function_1` call - the pre-export-fn dispatcher - which a thin wrapper could replace.

## Why a wrapper is not enough

The old module in that experiment carries the named parameter list, so the mapping to the current `i[]`/`f[]` layout
needs no guessing: every index is documented by native field name. And that is exactly what reveals the real problem:

```
old harness parameters:  37, none for moe.drop_cold_frac or moe.substitute_lambda
current module:          rules at f[3] and f[4] (lines 278, 282, 285)
```

The current module validates two fields the harness cannot set. Two of its rules - `drop_cold_frac` in (0,1] and its
requirement for a cache - read `f[3]`, and a third reads `f[4]`. With the harness passing zeros for both, the
`!(f[3] >= 0.0 && f[3] <= 1.0)` rule is satisfied by zero, but any case the old gate considered "valid" is only
comparable for the rules that existed when it was written.

So the gate is not a stale *binding* - it is a stale *configuration space*. Relinking it would produce a number whose
disagreements could not be attributed between "the artefact differs" and "the harness cannot express the input".

## What is required, and why it is not done here

The gate has to be extended with the two new fields and their native counterparts before it can be used as an
artefact check, and then re-run over 4000 cases. That is real work on a harness, not a relink, and it does not belong
in the last minutes of a session - the failure mode this whole report keeps recording is a measurement made cheaply
and interpreted as if it were made properly.

Config validation's existing evidence stands: 4000 configurations, 0 failures, 0 uncovered rules, twice. What it does
not cover is the artefact, and that gap now has a precise cause rather than a vague one.
