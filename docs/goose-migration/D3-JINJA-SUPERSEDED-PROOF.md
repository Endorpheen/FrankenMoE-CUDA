# D3: the eight Jinja slices are superseded, and here is the proof

Measured 2026-11-19 on the working tree, so the deletion that follows needs no judgement, only a
command.

## The eight files

`goose/jinja/{parsers,streaming,typed,autoparser,delimiters,json-escape,tool-builders,jinja-macro-call}.goose`

## Three facts, each checkable in seconds

1. **Every exported door exists in `goose/jinja/runtime.goose`.** 540 export declarations across the
   eight files, 92 distinct names, and the set difference against `runtime.goose` is empty.
2. **534 of those bodies are byte-identical** to the runtime's, and the 6 that differ are two doors
   only - `chat_standard_constructed_tools` and `chat_standard_json_tools` - where `runtime.goose`
   carries `sb_schema_name(sb, st, i)` as an extra argument that the slices do not have. The runtime
   is the newer copy; the slices are older, not alternative.
3. **Nothing references them.** None of the eight appears in the module list of
   `tools/build-main-runtime.sh`, and no `.goose` file in `goose/` imports any of them.

They are untracked in Git, so they were never committed: their presence changes nothing, and their
removal changes nothing either.

## Why they are not deleted here

Rule 26: a tool that deletes files it did not create should refuse, and work that is not recorded
verifiably does not exist. These files are unrecorded, so the deletion is left as one explicit
command rather than a side effect of a cleanup commit:

```
rm goose/jinja/{parsers,streaming,typed,autoparser,delimiters,json-escape,tool-builders,jinja-macro-call}.goose
```

Whoever runs it can confirm beforehand with the three checks above; nothing else in the tree changes.

## What D3 still needs after this

The label `SUPERSEDED` in `tools/sync-goose-tree.py` already points the eight at `jinja/runtime.goose`,
so a future sync will not reintroduce them. What remains for D3 is the same thing every other P11-04
slice needs: the native Jinja path in C++ being unreachable on the shipped call path, which is a
measurement on a build, not a file count.
