# The accepted runtime reproduces from neither path in the tree

Measured 2026-11-19 by building both and comparing with `tools/compare-artefacts.sh`. This is the
decisive result of the day's investigation and it replaces every earlier statement about
reproducibility.

## The three artefacts

| artefact | producer | sha256 | bytes | defined symbols | unit doors |
|---|---|---|---|---|---|
| accepted release EXP1476 | **not recorded anywhere** | `0ffd98c2` | 5 758 048 | 6085 | **198** |
| path A: `build-independent-apex.sh` → `build.py` | in the tree, runs | `bea5d6af` (working dir) | 5 788 128 | 6090 | **200** |
| path B: `consolidate` + `stage-m1-tree` + `build-from-tree` | in the tree, runs | `cbe39146` | 5 685 824 | 6525 | **11** |

Path B, built today from `main` with all 18 extra objects present and their producers recorded:
192 doors the release has are absent, and 5 doors it lacks are present (`q35_kind`, `q35_plan`,
`q35_rope_multi`, `q35_shape`). It has 440 more defined symbols than the release, so it is not a subset
of it either - it is a differently composed runtime.

## What this means, stated plainly

1. The accepted artefact is **not reproducible from the repository** by any script it contains. The
   command that produced it is not recorded, and neither surviving path matches it.
2. Rule 22 says an accepted set that cannot be rebuilt cannot be replaced. Taken literally, that means
   EXP1476 cannot be superseded until a producer for it is recovered or a new artefact is accepted in
   its place on its own evidence.
3. The two paths are not variants of one build: A links 10 Goose modules from source, B links the
   composed 156-module unit, and the release matches neither component set. The most likely explanation
   is that EXP1476 was produced by a third, ad-hoc assembly during the release push, which is exactly
   the kind of step that leaves no record - and exactly why `BUILD-COMMAND` now exists.

## What is proven, and what is not

Proven: the two pipelines in the tree run to completion from tracked sources (A with `check_carriers`,
B with 11/11 patches, 36 modules, 160 bridges, 44 flint objects, 18 extra objects); every input of B has
a named producer; the component check passes for A, B and the release.

Not proven: that any of the three behaves the same as another. **No measurement was taken today**, and
a component list is not behaviour.

## What the next session must do, in this order

1. Decide which artefact is the baseline going forward. If it is EXP1476, its producer must be
   recovered (the release was pushed at 2026-11-19 19:00; the shell history and the CI log of that
   window are the only untried sources).
2. If instead a new artefact is to become the baseline, it must pass the accepted-component check, carry
   the full door set with an explanation for each absence, then take the fixed-profile Apex run with
   paired repeats at 128/256/512 tokens, and only then be called the new baseline.
3. Only after that does deleting native code become legal again (rule 15), and only for the components
   the accepted artefact demonstrably carries.
