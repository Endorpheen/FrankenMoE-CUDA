# FrankenMoE migration status

Updated: 2026-11-14 (2). This file is the current state; update it in the same change
as every accepted slice.

## Ledger

`docs/goose-migration/tasks.json`: **DONE 103**, **PARTIAL 7**, **TODO 42** of
152 functional requirements. Slice front: **74 / 88** done, 14 remaining.
Do not read the DONE count as project percentage; it counts requirements, not
code or time.

The remaining partial requirements and their slice fronts:

| requirement | slices | what is open |
|---|---|---|
| P6-09 | 1 / 5 | LoRA manifest, LoRA apply, saver writer, saver from model |
| P7-02 | 2 / 5 | tensor data (in acceptance), split/merge, model saver |
| P7-04 | 2 / 6 | imatrix loader, unweighted quantizers, weighted quantizers, dequant closure |
| P7-08 | 4 / 8 | plan-level split: chat apply and time injection, hoisted native names |
| P7-11 | 5 / 5 | acceptance wording only; all five slices are accepted |
| P8-07 | 2 / 5 | MCP session core, decision core, integration shells |
| P9-06 | 4 / 4 | acceptance names the session-loop budget/interrupt behavior |

## What the canonical tree contains

`goose/` holds 4107 verified files. The accepted slices of this phase cover:

- Jinja runtime 19025 lines (expressions, statements, filters, tests, macros,
  chat layer, streaming, delimiters, autoparser, tool builders, nine parser
  families) with differential gates;
- JSON/schema/PEG/GBNF: parser, dump, schema-to-GBNF converter, four tool gates;
- chat: parse params, thinking control, CLI line protocol, arena load;
- GGUF metadata context (writer, tensor data, split/merge still native);
- quantization type selection and allow-cuts (imatrix loader still native);
- HTTP URL policy, HF cache naming, download selection, log policy, console
  editing (P7-11 complete);
- server tool definitions and request schema;
- twenty model architecture builders over shared model-graph and family modules (P6-05 closed).

## Sources and evidence

- Sources: [`goose/`](../goose/README.md), grouped by subsystem.
- Map: `goose/MANIFEST.json` (experiment, native path, gate, SHA256 per file).
- Tools: `tools/sync-goose-tree.py` rebuilds the tree,
  `tools/check-goose-tree.py` verifies hashes and experiments (green).
- Every accepted slice has a card under `experiments/EXP-*` with the gate
  script, mutation table and recorded results.

## Composition debt

An export-name audit over the 34 canonical files finds five shared names that a
single linked runtime must deduplicate: `builtin_call`/`builtin_names`
(builtins vs runtime), `value_cmp`/`value_info` (value vs value-json) and
`json_parse` (the parser plus three byte-identical module copies required by
Goose's name-based import). Every standalone gate is green; deduplication is a
composition step and must be done with the gates re-run.

The GGUF writer is the next composition-sensitive piece: the writer core is a
separate slice, and its byte-exact gate must be added before split/merge can
depend on it.

## Open documentation debt

1. The root `README.md` still describes the pre-migration C++ runtime; it has a
   migration pointer at the top, but the results sections need a rewrite.
2. `docs/goose-migration/HANDOFF.md` is a historical narrative (EXP237-244), not
   the current state; this file replaces it as the status source.
3. 26 slice files have no native target in the ledger and sit under `misc/`.
4. Older experiments still track build artefacts committed before the storage
   rule; a cleanup needs Igor's explicit approval and must not delete sources.

## Next

1. Accept the in-flight slices: seven P6-05 architectures, imatrix loader,
   LoRA manifest, GGUF writer.
2. Then P8-07 S3/S4 (MCP session core, decision core) and P6-09 S2.
3. P7-08: hoisted native names need a native insertion index per node; chat
   apply and time injection are the remaining chat-layer work.
4. Keep `goose/` and this file current with every accepted slice.
