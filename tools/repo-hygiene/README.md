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
