# EXP-2026-11-19-1328: the accepted writer cases run against the shipped library

## What was done, and what was deliberately not

The GGUF writer's evidence already existed: EXP-2026-11-14-601 accepted it with `cases=41 checks=307 failures=0`
against the module's own build. Rule 33 asks a different question - does that behaviour survive in the **cumulative
artefact** - and the answer was one compile away, because the harness is a source file in that experiment.

`fixture.cpp` now links against `builds/flint-acc/bin/libllama.so` instead of the module's local build:

```
FG_G3 cases=41 checks=307 failures=0
```

Same cases, same checks, same verdict, from the shipped library. Nothing was re-implemented; the recorded harness was
pointed at the other artefact.

## Why this is the useful pattern

Every module this session composed into the unit had one question left open: does it still behave as accepted now that
it lives in the binary everyone runs? For modules whose harness takes the module as a link input, the answer costs one
`g++` line. That is cheaper and stronger than re-deriving cases, and it is the check rule 33 actually asks for.

## Wired into the sweep

`experiments/EXP-2026-11-14-601-p7-02-gguf-tensors/verify.sh` runs it, and the verification sweep now reports
**19 passed, 0 failed, 1 stale**. The runner needed one fix to accept a directory that has only `verify.sh`: it
skipped such directories before reaching the check.

## What this does not establish

That the writer is *used*: the saver still writes natively. This is the same distinction as everywhere else in this
report - proven behaviour in the artefact, no call path yet.
