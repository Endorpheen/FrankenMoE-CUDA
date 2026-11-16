# sync-goose-tree.py — the rewrite guard (rule 26)

## What happened

`tools/sync-goose-tree.py` rebuilds `goose/` by removing its subdirectories and
copying from the experiment sources. On 2026-11-16 a run was started while the
tree held work that was not committed in `goose/`. It replaced three canonical
files with older experiment copies:

- `goose/model/qwen35moe.goose` lost the IMRoPE block (`fg158_rope_multi`,
  `q35_rope_multi`) — 25 lines;
- `goose/cpu/goose-graph-builders.goose` lost 45 lines;
- `goose/cuda/goose-mmq-dispatch.goose` lost 12 lines;
- `goose/MANIFEST.json` was rewritten with the older hashes.

The three files and the manifest were restored from `HEAD` immediately; only the
intended `test-runner` change was kept. No damaged state was committed. The two
tool-generated duplicates (`server/p8-12b-server-switches.goose`,
`server/p8-12c-mime-cache-tools.goose`) were removed because they were
reproducible copies of experiment sources and were not in the manifest.

## The guard

`build()` now refuses to rewrite `goose/` when `git status --porcelain -- goose
:(exclude)goose/_archive` is non-empty. It prints the uncommitted paths and
returns 1. `--force` overrides. `_archive` is no longer removed by the rebuild.

Verified:

- `python3 tools/sync-goose-tree.py` → rc=1, one refusal line, no tree change;
- `python3 tools/sync-goose-tree.py --check` → reports drift only (rc=1), also
  no tree change.

## Remaining tool issue (not fixed here)

The rebuild also copies every `.goose` under `experiments/` that is not in
`CANONICAL_EXPERIMENTS`, which is how the two stray `server/p8-12*` files
appeared and why they were not in the manifest. That behaviour needs a separate
decision: either give those copies manifest entries, or copy only the declared
build set (`docs/goose-migration/BUILD-SET.md`) and shared modules.

Until then, run the tool only after `git status --short` is clean in `goose/` or
after moving unrecorded sources outside the tree — the guard enforces the first
part.
