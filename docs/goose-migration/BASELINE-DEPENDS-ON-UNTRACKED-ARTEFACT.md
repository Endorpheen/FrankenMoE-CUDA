# The accepted checkpoint cannot load without an untracked experiment artefact

Found 2026-11-19 while comparing the checkpoint with the release set, file by file. This is the most
concrete defect of the day and it is not about provenance style - it is about the baseline failing to load.

## The facts

| fact | evidence |
|---|---|
| checkpoint has 23 files, the release set has 24 | file-by-file sha256 comparison |
| the missing file is `libmoe_mmid_tiled.so` | it is the only difference |
| the other 23 files are byte-identical | same comparison |
| `libllama.so.0` **requires** that file | `ldd` lists it as a dependency |
| it resolves to an experiment build directory | `libmoe_mmid_tiled.so => /home/end0/AI/FrankenMoE-CUDA/vendor/flint/../../experiments/EXP-2026-11-18-1008-p1-12-gemv/build/p112-mmid-v3/libmoe_mmid_tiled.so` |
| that file is **not tracked by Git** | `git ls-files --error-unmatch` -> no match |

So the accepted artefact's own library points into
`experiments/EXP-2026-11-18-1008-p1-12-gemv/build/p112-mmid-v3/`, a directory that rule 36 says must not
hold committed build output. If that directory is cleaned - which this project's own practice requires -
`libllama.so.0` stops resolving a dependency and the baseline stops running.

## Why the release is not affected in the same way

The release set carries the library as a file, so it loads from its own directory. That is why the release
works and the checkpoint does not: the release was assembled by copying the library in, and the checkpoint
was not.

## Consequences

1. `builds/checkpoint-20261119-EXP1476` is **not a runnable set on its own**; it is 23 files plus a
   dependency on an untracked directory.
2. Every statement that treats the checkpoint as the accepted artefact - including the provenance checks -
   is about files, not about a loadable runtime.
3. The RUNPATH defect recorded earlier (absolute build paths in all 12 libraries and the binary) is the same
   root cause seen from the other side: the link points into a directory rather than at `$ORIGIN`.
4. Rebuilding does not fix it by itself, because path B also links with `-Wl,-rpath,"$TREE/libs"`.

## What fixes it, in order

1. copy `libmoe_mmid_tiled.so` into the checkpoint so the set is self-contained, and verify with
   `ldd` plus a hash entry in `SHA256SUMS`;
2. relink with `$ORIGIN` instead of an absolute tree path, so the set works from any directory;
3. only then re-run the release-integrity check, because a set that cannot load has not been verified even
   if its hashes match.

None of this was done here: step 1 changes an accepted set, which rule 22 forbids without Igor's explicit
approval, and step 2 is a rebuild.
