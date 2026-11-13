# Repository hygiene: broken symlinks

Inventory taken 2026-11-12 on the working tree:

| measurement | value |
|---|---|
| broken symlinks under `runtime/` and `experiments/` | **27 160** |
| of those tracked by git | **0** |
| biggest sources | `runtime/exp220` 1760, `exp196` 1440, `exp195` 1426, `exp200` 1376, `exp183` 1336 |

They are leftovers from staging copies: each measurement directory was populated with symlinks into a build tree that has
since moved or been rebuilt, so the links point at paths that no longer exist.

## Why this matters

- a plain `find`/`du`/`grep` over the tree wastes time and produces the noise I hit while looking for a CPU library copy;
- `tar`/copy operations either follow them and fail or silently bloat;
- the repository rule says to keep the tree tidy and not leave stray artefacts.

## How to remove them

    tools/repo-hygiene/clean-broken-links.sh            # dry run, prints counts
    tools/repo-hygiene/clean-broken-links.sh --apply    # deletes untracked broken symlinks

The script re-checks git tracking for every path before deleting, so anything tracked is skipped. Because deleting 27k paths
is not reversible and this is not a change to code, it was prepared but **not applied** - that needs an explicit go-ahead.

# Tracked build artefacts: 11.7 GiB in the index

Inventory 2026-11-12:

| measurement | value |
|---|---|
| tracked paths matching `/build/`, `*.so.0`, `*.bin`, `*.o` | 370 |
| their combined size | **11 672 MB** |
| of those under a `/build/` directory that `.gitignore` already excludes | 268 |
| largest single files | `good.bin` 370 MB (two copies), `libggml-cuda-*.so` 210 MB each |

So the tree emits build output into `experiments/*/build/` and `runtime/*/`, `.gitignore` has `/build/`, and yet hundreds of
these files were committed before or around that rule. The repository therefore carries ~11.7 GiB of build output in its
history, which is exactly what the "keep the repository tidy, no build rubbish" requirement forbids.

## Two levels of remedy, different risk

1. **Stop tracking now (reversible).** `git rm -r --cached` on the 370 paths keeps every file on disk, removes it from the
   index, and one commit makes future diffs clean. History still contains the blobs, but nothing new accumulates.
2. **Rewrite history (irreversible).** `git filter-repo --path-glob '*/build/*' --invert-paths` would drop the blobs and
   shrink `.git` substantially. That rewrites every commit hash on this branch and must not be done without an explicit
   decision - especially since the branch is 580 commits ahead of origin and other agents read these commits.

Prepared but not executed: choosing between level 1 and level 2 is an owner decision, and level 2 is destructive by nature.

## Recommended order

Level 1 first (safe, immediate), then decide on level 2 separately if the `.git` size actually matters for the workflow. Not
doing anything at all is the one option that keeps getting worse with every build.
