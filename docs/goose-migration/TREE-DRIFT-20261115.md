# Goose tree drift — found 2026-11-15

Recorded, not fixed: the repair touches ~800 files that are not mine to rewrite
(rule 5) and needs a decision on whether the manifest or the tree is authoritative.

## What the two checks say

`python3 tools/check-goose-tree.py` — 8 problems, 20 warnings:

| kind | file |
|---|---|
| hash mismatch | `model/qwen35moe.goose` |
| hash mismatch | `cpu/goose-graph-builders.goose` |
| hash mismatch | `misc/goose-tokenizer.goose` (also has uncommitted edits by another agent) |
| hash mismatch | `cuda/goose-mmq-dispatch.goose` |
| not in manifest | `cpu/goose-moe-router.goose` |
| not in manifest | `model/gdn-ops.goose` |
| not in manifest | `model/qgate-split.goose` |
| differs from its source | `model/qwen35moe.goose` vs EXP-2026-11-14-577-p6-05-qwen35moe |

`python3 tools/sync-goose-tree.py --check` — **802 of 802 slice files** reported as drifted.

## Why this matters

Three accepted, committed modules — `cpu/goose-moe-router.goose`, `model/gdn-ops.goose`,
`model/qgate-split.goose` — exist in the tree and are **absent from the manifest**. The manifest is what
`migration-status.sh` and the coverage accounting read, so the reported module counts are not counting
three real modules. The MoE router in particular is the decision half of `build_moe_ffn`, which is
central to the Apex path.

The four hash mismatches mean the tree no longer matches the slices the manifest names. Either the files
were edited after acceptance without a new slice, or the manifest was regenerated against a different
tree. Neither is currently distinguishable from the repository alone.

## Why it was not caught

`.githooks/pre-commit` runs only `tools/check-generated-files.py` — the generated-C size limit. It does
not run either Goose consistency check, so a commit can change or add a Goose file without the manifest
noticing.

## Proposed fix, in order

1. Decide which side is authoritative. If the tree is (likely — it is what compiles), regenerate the
   manifest with `tools/sync-goose-tree.py` and review the 802-file diff as one commit whose message says
   it is a metadata resync, not a code change.
2. If any of the four mismatched files were edited after their acceptance, that edit needs its own slice
   and gate rather than being absorbed into a resync. `misc/goose-tokenizer.goose` currently has
   uncommitted edits by another agent and is out of scope (rule 5).
3. Add `tools/check-goose-tree.py` to `.githooks/pre-commit` so the drift cannot grow silently again.

## Not done here

No regeneration, no manifest edit, no commit of another agent's file. The 802-file diff is larger than
anything this session should do unattended.
