# Correction: the kernel object exists, and the gate passes

This card claimed that `libmoe_mmid_tiled.so` "exists nowhere in the tree" and that the gate expected an
artefact the design no longer produces. **Both statements were wrong**, and the error was in my search: I ran
`find` over `builds/ runtime/ releases/ Backup/ integration/ reference/` and not over `experiments/`, where the
P1-12 build keeps it.

```
experiments/EXP-2026-11-18-1008-p1-12-gemv/build/p112-mmid-v3/libmoe_mmid_tiled.so
sha256 1821f513266ade87f316063555e80750a515662743d80e4b2b696452a0c4726f
```

That hash is the one the P1-12 record names as the accepted Goose kernel object, and the running server links it
(`ldd` resolves it from that path). Copying it into the run directory is the whole fix:

```
ok P1-12 Goose Q4_K tiled kernel dispatch
ok P1-12 Goose kernel reference
ok P4-13 Goose allocator primitive
ok Goose kernel object present
CUMULATIVE-BASELINE PASS
```

## What was right and what was wrong

Right: the run directories did not contain the libraries, so the gate could not pass on them, and the paired
harness now snapshots all four artefacts - including this one, from its producer rather than from cmake.

Wrong: the conclusion that the artefact no longer exists, and the proposal to change the gate's rule. **The gate
was correct all along**; the measurement was incomplete. Changing it would have removed the check that noticed.

## The lesson, and it is the seventh of the session

A search that does not cover the whole tree produces a confident "does not exist". `find` over five guessed
directories is a guess about where a thing lives, and this project keeps its build products in experiments.
