# clean-checkout

**Requirement:** P10-06 - a clean isolated build with the pinned compiler.
**Produced by:** `tools/verify-clean-build.sh --compose-only`; permanent since 2026-11-19.
**Tier:** fast - a fresh worktree, compose and typecheck, about two seconds.

## What it proves

Three things, in the order that makes them meaningful:

1. the compiler the pin names is **the one on disk** - the check compares SHA-256 with
   `runtime/compiler/current.json` and refuses on a mismatch;
2. a **fresh worktree of HEAD** (nothing from the working tree) is self-sufficient: composing
   `qwen4exp`, `q4m`, `q4t` and `q4e` needs no file that exists only in the working tree;
3. that composition **typechecks**: 153 specializations.

It also prints how many tracked files in the working tree are modified, so a reader cannot mistake
this for a build of uncommitted work.

## What it does not prove

That the whole cumulative unit compiles from a clean checkout. That is
`bash tools/verify-clean-build.sh` without `--compose-only`, which composes, generates and compiles
all 160-odd modules - minutes of work, and the reason this test runs the cheap half every time and
hands the expensive half over.

## Run

```
bash tests/integration/clean-checkout/run.sh                      # the cheap check
bash tools/verify-clean-build.sh                                  # the full clean build
```
