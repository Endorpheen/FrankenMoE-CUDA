# Why restoring build.py did not restore the runtime

`runtime/reconstructed/build.py` does not build the extra objects. It links **whichever of them happen to exist**:

```python
bridge_objs += [str(x) for x in extra_mod if x.exists()]
```

Only `slru-policy.o`, `slru-runtime.o` and `slru-shim.o` are checked for existence, and the check prints a reminder
rather than rebuilding them. Every other object in `extra_mod` - `b291.o`, `fx286pic.o`, `fx262a.o`,
`fx263-pic.o`, `gates.o`, `fg-ar.o`, `fg-ar-bridge.o`, `fg-q35.o`, `fg-qg.o`, `fg-q35-bridge.o`,
`fg-plan-ffi.o`, `fg-gr.o`, `fg-gr-bridge.o` - is silently skipped when absent and silently included when present.

## Consequence

Restoring `build.py` to `c5e6ff02` restores the *list*, not the *composition*. What gets linked is determined by
directory state, which changes every time an experiment rebuilds one object and leaves it there. So:

- two checkouts of the same `build.py` can produce different binaries;
- a rollback to a commit cannot restore a binary unless the object directory is restored with it, and those objects
  are not tracked (git-ignored build products);
- the size difference observed after the rollback - 5660824 bytes now against 5652072 at `c5e6ff02`, +8752 - is
  consistent with a changed object set or with objects rebuilt from the same sources by different commands, and
  **cannot be resolved from the repository**, because the objects are not in it.

## What this does not say

It does not establish a cause for the throughput difference, and it is not a profiling result. It is a statement
about why "sources plus build.py match the commit" is not the same claim as "the runtime is restored", which is
exactly the distinction Igor drew.

## What would make a rollback real

1. Record the exact command and flags for every object in the composition, next to the object, so it can be
   regenerated identically (the bridge command is already in `build.py`: `gcc -O3 -DNDEBUG -fPIC -std=c11`;
   the `fg-*.o` and `slru-*.o` objects are produced by separate experiment scripts).
2. Fail the build when a listed object is missing, instead of silently linking without it. A silent omission is the
   same class of fault as the batch's silent row: the build succeeds and the behaviour is different.
3. Keep the object set that produced a confirmed measurement, with its hashes, so a later comparison has a
   reference that is not a rebuild of similar sources.

None of the three is done yet. Until they are, a rollback is a claim about files, not about the runtime.
