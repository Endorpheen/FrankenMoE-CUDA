# extra-objects-provenance

**Requirement:** P10-06 (reproducible build) and rule 22 (an accepted set must be rebuildable).
**Tier:** fast - reads a JSON file and stats paths. No build, no model.

## What it checks

`tools/consolidate-runtime-tree.sh` copies 18 hand-built objects into the tree instead of compiling
them. That directory was the last unknown in reproducing the accepted runtime: on 2026-11-19 it took a
symbol-by-symbol pass to show that 17 of the 18 come from tracked sources and one is the Goose
compiler's own runtime.

`docs/goose-migration/EXTRA-OBJECTS.json` records each object with its producer. This test fails when:

- an object in the registry names a source that no longer exists;
- a future object is added without a producer.

## What it caught on its first run

`fg-q35.o` was recorded as coming from `goose/model/qwen35-split.goose`, which does not define
`q35_plan`. The real exporter is `goose/model/qwen35moe.goose:124`, and the registry was wrong until the
test said so - which is the point: a provenance record that is never checked is a claim, not a record.

## Run

```
bash tests/regression/extra-objects-provenance/run.sh
```

Exit 0 with 18 of 18; exit 1 with the missing entry named.
