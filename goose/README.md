# goose/ - every Goose source in one tree

This directory is the single place to read the Goose implementation. It
contains every `.goose` source produced by the migration: 4062 files at the
time of writing.

## Layout

```
goose/common/jinja/lexer.goose        canonical module (one accepted file per native file)
goose/common/jinja/parser.goose
goose/common/jinja/value.goose
goose/common/jinja/value-json.goose
goose/common/jinja/builtins.goose
goose/common/jinja/runtime.goose

goose/ggml/src/ggml-cpu/<EXP>/...     slice sources, grouped by the native directory
goose/ggml/src/ggml-cuda/<EXP>/...    named by the ledger entry for that experiment
goose/src/<EXP>/...
goose/common/<EXP>/...
goose/tools/server/<EXP>/...

goose/_unmapped/<EXP>/...             experiments the ledger cannot map to a native path
goose/_external/<checkout>/...        sources that existed only in another LlamaServer checkout
goose/_external/_branches/<branch>/...  sources that existed only on an unmerged branch
```

The native directory comes from `docs/goose-migration/tasks.json`: the sources of
every task whose evidence names the experiment. Slice sources are kept under the
experiment that proved them because they are partial implementations of a
native file, not a drop-in replacement.

`_external` is an archive, not a working tree. It exists so that deleting an
old LlamaServer checkout cannot lose anything. Duplicate content is stored once.

## Rules

1. New accepted work is placed here in the same change that accepts it. The
   experiment stays the frozen proof: fixture, build script, mutation harness
   and numbers.
2. `MANIFEST.json` records, per file, the native path (best known), the
   experiment, the ledger task, a status (`ported`, `slice`, `external`) and a
   SHA256.
3. No placeholders for modules that are still native: coverage is shown by the
   absence of a file, not by a stub.

## Regenerate and check

```
python3 tools/sync-goose-tree.py          # rebuild the tree and the manifest
python3 tools/check-goose-tree.py         # verify hashes, experiments and coverage
```

`sync` deletes the generated slice, `_external` and `_unmapped` directories and
rebuilds them from `experiments/` and the external checkouts, so the tree and
the manifest cannot drift. The canonical `common/jinja/*.goose` files are
preserved across a rebuild.
