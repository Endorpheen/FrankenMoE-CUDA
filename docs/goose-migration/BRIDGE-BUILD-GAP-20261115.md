# The MoE-cache Goose bridges are built outside every build script

Found 2026-11-15 while checking whether the migration's code is actually reachable. Recorded because it
is a reproducibility gap, not a defect: everything works, but two of the links that make it work cannot
be rebuilt by any script in this repository.

## What was verified to work

The Goose decisions in the MoE cache hot path **do execute**. Calling them directly out of the shipped
library:

```
fg263c_run_from_slot  slots=[100,101,102,103,-1]
  from 0..3 -> 4   (correct: four consecutive slots)
  from 4    -> 0   (correct: not cached)
```

A rebuild of `libllama-exp371.so.0.7.0` keeps 325 `fgNNN` symbols and the same answers, so the current
tree builds and links.

## The gap

`runtime/reconstructed/build.py` links four bridge objects from prebuilt files rather than from source:

```python
extra_mod = [W/'b291.o', W/'fx286pic.o', W/'fx262a.o', W/'fx263-pic.o', W/'gates.o']
```

Those `.o` files are dated **Nov 12** and no script in `tools/` or `experiments/*/build*.py` produces the
exact names `fx263-pic.o`, `fx262a.o`, `b291.o`, `fx286pic.o`. The Goose sources they came from
(`goose/cuda/goose-moe-cache-copy-decisions.goose`, `goose-moe-cache-admission.goose`) are **not in any
build module list** - `grep` over `build.py`, `tools/build-main-runtime.sh` and
`tools/build-standalone-runtime.sh` finds no reference to them.

So: the two decisions that dominate decode performance (the expert cache nearly doubles decode, EXP-752)
are served by objects whose provenance is not reproducible from the repository.

## It is reproducible in principle

`experiments/EXP-2026-11-12-264-moe-cache-copy-integration/build_and_check.py` rebuilds the bridge from
`bridge.c` and confirms the ABI:

```
undefined added: ['fg263c_enabled', 'fg263c_run_from_pairs', 'fg263c_run_from_slot', 'fg263c_stats_class']
PASS integration object compiles with only fg263c_ undefined symbols added
```

And `goose check goose/cuda/goose-moe-cache-copy-decisions.goose` typechecks (11 specialisations, 0
errors). The generated C is produced under `~/.goose/build/prog/<module>-<hash>/`. So the missing piece is
only the script that turns the Goose module into `fx263-pic.o` and drops it at that path.

## Why this matters for rule 15/17

The migration's acceptance criterion is that the Goose runtime answers a capability without the old C++.
Right now an accepted capability (MoE cache copy run-length and admission) is answered by Goose code that
the repository cannot regenerate. That is not the same as "answered by the tree as committed" - the
committed source and the shipped object can drift apart silently, and nothing would notice.

## Proposed fix

1. Add the two modules to a build step that emits C from the `.goose` sources, compiles it with the
   bridge, and writes the object the linker expects. `build_and_check.py` in EXP-264 already does the
   hard part; it needs to become a `tools/` script with a stable output name.
2. Record the source hash next to the object so drift is detectable, the same way `bench-log.py` records
   library hashes.
3. Only then is it safe to change either module - which is exactly what active migration needs.

## Not done here

No build change and no object was replaced. Replacing a working hot-path object without a way to compare
it against the current one would be the opposite of careful.
