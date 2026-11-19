# EXP-2026-11-19-1329: which recorded harnesses can be pointed at the shipped library

## The question

EXP-2026-11-19-1328 pointed one experiment's accepted fixture at `libllama` from the shipped build and it ran green
(`cases=41 checks=307 failures=0`). Is that a general trick? A scan of every `experiments/*/tests/fixture.cpp` that
declares its doors inline:

```
fixtures examined: 84
fully linkable against the shipped library: 0
blocked by symbols outside the unit: 84
```

## Why, and what the criterion is

Each of those 84 fixtures declares the exports of **its own experiment's module** - `fg289dh_run`, `fg296k_fail`,
`fg212s_top_k_cut` - and every one of those names is absent from the shipped library, because those modules are not
composed into the unit. Pointing such a harness at the shipped library cannot work by construction.

EXP-2026-11-14-601 is different in two ways that together define the criterion:

1. its module (`gguf-tensors`) **is** composed into the unit, so the symbols exist in the artefact;
2. its fixture binds through the module's generated **header** rather than inline declarations, so the same source
   compiles against either build.

**The pattern applies when the module is in the unit and the harness binds through the composed header.** For the
other 84 it does not, and the correct action is to leave them where they are rather than to weave their modules into
the unit for the sake of a check.

## What this says about the sweep

`tools/run-verification.sh` covers 19 harnesses: those that verify decisions against native code, plus the writer
check that runs against the shipped artefact. The 84 older fixtures are not convertible and are not counted as
coverage of the shipped runtime - they are evidence about their own modules at the time they were written.

## Next

The composed modules that could gain a shipped-artefact check through this pattern are the twelve named by
`tools/check-module-coverage.py`; the writer is the first of them to get one.
