# EXP-2026-11-19-1294: the tree after freeing 12.8 GiB

The disk was full (793 MiB free of 938 GiB). Regenerable CMake trees from past experiments were removed:
`build/exp*`, `build/llama-cuda`, `build/franken-cuda`, `build/ornith-upstream-*`, `build/goose-main-freeze-*`.
Nothing tracked, nothing in `work/`, nothing in `Backup/`, no accepted library set.

```
free before   793 MiB
free after  14 GiB
gates       cumulative-baseline PASS, unwired-modules PASS, unresolved-symbols ok, abi-pairs ok
suite       18 passed, 0 failed, 1 stale
```

## What was deliberately not removed

- `work/` - other people's in-progress changes (hard rule 5);
- `Backup/` - the restore points (rules 22 and 24), including a 9.9 GiB recovery set that needs a decision;
- `builds/flint-acc` and `build/sampler-chain` - the working build;
- `experiments/*/build` for the active P6-10 experiment - it holds `libmoe_mmid_tiled.so`, the artefact
  `check-cumulative-baseline.py` verifies. Deleting it would have broken a gate while freeing under one gigabyte.

That last point is the reason this card exists: the biggest items are not the safest ones, and the check that
matters after a destructive cleanup is the one that reads the artefacts back.
