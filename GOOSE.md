# Where the Goose sources live

All Goose sources are consolidated in one tree: [`goose/`](goose/README.md).
It holds 4062 `.goose` files and mirrors the llama.cpp layout, with partial
slices grouped under the experiment that proved them:

```
goose/common/jinja/lexer.goose      canonical module (complete port)
goose/ggml/src/ggml-cpu/<EXP>/...   slice sources under their native directory
goose/ggml/src/ggml-cuda/<EXP>/...
goose/src/<EXP>/...
goose/_unmapped/<EXP>/...           experiments with no native target in the ledger
goose/_external/...                 sources that existed only in other checkouts
```

`goose/MANIFEST.json` records, for every file, the native path it belongs to,
the experiment, the ledger task, the status and a SHA256. `goose/README.md`
explains the rules; `tools/sync-goose-tree.py` rebuilds the tree and
`tools/check-goose-tree.py` verifies it.

The experiment directories keep the proof: the differential fixture, the build
script and the mutation harness. They are no longer the place to read the code.

Coverage check: every `.goose` file under `experiments/` and under every
LlamaServer checkout has a content-identical copy in `goose/` (6340 files
checked, 0 missing).
