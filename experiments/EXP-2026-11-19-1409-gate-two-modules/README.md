# EXP-2026-11-19-1409: the two newly composed modules are protected by the gate

## Why this is a step and not bookkeeping

The fit-params line and the bench-report formatting were composed into the cumulative unit in the previous two cards.
Composition is a build-set fact, and a build-set fact can silently revert - the module list is one line, and a rebuild
from another recipe drops it. Nothing would have noticed: the doors are unused, so no runtime behaviour depends on them
and no test fails.

The component gate is exactly the mechanism for that: an entry is a promise the artefact must keep.

## What was added

```
("goose-fit-line",     "nm", "fp_line",      ...)
("goose-bench-report", "nm", "lb_csv_escape", ...)
```

## Verified to pass, and to fail

Current library:

```
present  goose-bench-report
check-accepted-components: ok, every accepted component is in the shipped library
```

Against the EXP1405 checkpoint, which predates both modules:

```
MISSING  goose-bench-report  <- ...
check-accepted-components: FAIL, 2 accepted component(s) absent: goose-fit-line, goose-bench-report
```

So the entries are not decorative: pointing the gate at an artefact without the modules names both by name and fails. The
pre-flight in `tools/apex-test.sh` runs this gate before any benchmark, which means a build missing these modules cannot
produce a measurement that looks valid.

## What this does not do

It does not connect the modules to a call site - no tool in this tree consumes either line. It protects the composition
until such a caller exists, which is the honest state of P10-02's first two items.
