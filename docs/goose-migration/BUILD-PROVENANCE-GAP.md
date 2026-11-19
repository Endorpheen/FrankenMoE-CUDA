# Two build paths, and the release does not say which one made it

Found 2026-11-19 by the candidate rejection recorded in `CANDIDATE-20261119-REJECTED.md`.

## The two paths

| path | script | what it composes | doors in the artefact |
|---|---|---|---|
| A | `runtime/reconstructed/build.py` (called by `tools/build-independent-apex.sh`) | 13 staged carriers, 36 bridge modules from `integration/modules`, 26 explicit bridges, **10 Goose modules from source** (`_GOOSE_PAIRS`: six unicode, qwen35-split, arch-registry, graph-reuse, qgate-split) | **87** |
| B | `tools/stage-m1-tree.sh` + `tools/build-from-tree.sh` | a staged tree: `$TREE/modules`, `$TREE/bridges`, `$TREE/flint-objects`, `$TREE/extra-objects`, linked with `-Wl,-rpath` into `$TREE/libs` | **198** (the accepted release) |

Path B never calls `build.py`, and path A knows nothing about a staged tree. A fresh run of A is what
this host has been asked to build all day, and its artefact is a different thing from the release.

## Why the difference is 131 doors

The set A lacks includes `arch_default`, `arch_resolve`, `arch_sm`, `cfg_validate`,
`fg_grammar_first_set`, `fg_grammar_reject`, `fg_dry_rep_limit_entry` - modules that path B links and
`_GOOSE_PAIRS` does not contain. The set A carries 20 doors the release does not, so the two are
different artefacts, not an old and a new version of one.

## What is wrong, precisely

Not that two paths exist - a reconstructed DSO and a staged-tree DSO can both be legitimate - but that
**the release records neither**. `MANIFEST.md` says "the cumulative Goose build accepted on
2026-11-19" and names the source revision; it does not name the producing command, so:

- "reproducible" cannot be checked by anyone, including its author;
- a component missing from a fresh build cannot be attributed to a build path or to a code change;
- the accepted set cannot be rebuilt from tracked sources, which rule 22 requires before it may be
  replaced.

## What was done about it

1. `tools/assemble-release.sh` now writes a `BUILD-COMMAND` file into every release, from
   `FG_BUILD_COMMAND` or the set's own file, and writes `unknown: ...` when neither exists - because
   an unrecorded producer is itself the finding.
2. The published release `v0.10.0-exp1476` received `BUILD-COMMAND` with exactly that `unknown` text.
   That is honest: the command is not recoverable after the fact.
3. Nothing else was changed. The release stays the baseline; path B is what must be run to reproduce it,
   and that is the next work item rather than another speculative build.

## Next, in order

1. run path B from the recorded revision and compare the produced `libllama.so.0` with the checkpoint
   by component list and by door count - a match proves reproducibility, a difference names the drift;
2. make the chosen path the only one a release may come from, and record it in `BUILD-COMMAND` from
   then on;
3. only after that compare performance, because a library from a different path is a different runtime.
