# goose/ - the Goose sources

Start here. Every Goose port produced by the migration is in this directory,
grouped by what it does rather than by the experiment that proved it.

## Groups

| directory | files | what is inside |
|---|---:|---|
| `jinja/` | 8 | the Jinja stack: lexer, parser, value model, JSON output, built-ins, runtime |
| `json/` | 33 | JSON text parsing, rendering, JSON-schema decoding and introspection |
| `grammar/` | 1 | GBNF grammar text parsing |
| `server/` | 44 | HTTP server decisions: routing, limits, SSE, providers, startup and shutdown |
| `cpu/` | 239 | CPU backends, quantized kernels, MoE, allocator and scheduler work |
| `cuda/` | 233 | CUDA operators: attention, quantization, reductions, RoPE, SSM, MoE |
| `kv/` | 90 | KV cache, memory, batch, context and recurrent state |
| `model/` | 29 | model loading, GGUF, tokenizer and graph construction |
| `sampling/` | 5 | sampler decisions and output selection |
| `chat/` | 14 | chat templates, tool calls and message conversion |
| `misc/` | 33 | slices whose native file the ledger does not name |
| `_archive/` | 3332 | sources preserved from other checkouts and branches, not part of the tree |

A complete module is one file named after the native module it replaces
(`jinja/lexer.goose` replaces `common/jinja/lexer.cpp`). A partial slice is
named after the decision it implements (`server/route-table.goose`,
`cuda/flash-attention/...`). The experiment that proved each file, its gate
result and its SHA256 are in `MANIFEST.json`.

## Reading the tree

1. Pick the group that matches the subsystem you care about.
2. A single `.goose` file is one slice or one complete module. A directory
   groups several files from the same slice.
3. `MANIFEST.json` maps the file back to the experiment, the native path and
   the differential gate.
4. `_archive/` exists only so that deleting an old LlamaServer checkout cannot
   lose anything. Do not treat it as current code.

## Rules

1. New accepted work is placed here in the same change that accepts it. The
   experiment keeps the proof: fixture, build script, mutation harness, numbers.
2. No placeholders for modules that are still native: coverage is shown by the
   absence of a file, not by a stub.
3. Edit `goose/`, not the copy inside an experiment, once a slice is accepted.

## Regenerate and check

```
python3 tools/sync-goose-tree.py     # rebuild the tree and MANIFEST.json
python3 tools/check-goose-tree.py    # verify hashes, experiments and coverage
```
