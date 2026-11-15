# FrankenMoE migration status

Updated: 2026-11-15 (12). This file is the current state; update it in the same change
as every accepted slice.

## Ledger

`docs/goose-migration/tasks.json`: **DONE 112**, **PARTIAL 14**, **TODO 26** of
152 functional requirements. Slice front: **139 / 139** done, none remaining.
Do not read the DONE count as project percentage; it counts requirements, not
code or time.

The remaining partial requirements and their slice fronts:

| requirement | slices | what is open |
|---|---|---|
| P6-11 | 3 / 3 | wire ngram_draft.cpp and session.cpp to the Goose decisions |
| P7-12 | 3 / 3 | wire the tokenizer dispatch, special-token partition and common strings |
| P8-06 | 3 / 3 | wire model_router.cpp to mg_/hr_/cn_ |
| P9-01 | 3 / 3 | wire validate(), arch_registry.cpp and main.cpp |
| P9-03 | 3 / 3 | replace the 25 streaming call sites |
| P9-04 | 3 / 3 | rewrite on_eval to the rh_ plan and replace the overlap/row-source call sites |
| P9-05 | 3 / 3 | extract expert_bytes_per_slot and wire rts_n_batch/rts_ubatch |
| P9-07 | 3 / 3 | wire the three sinks and DeterministicWorkload |
| P9-08 | 3 / 3 | wire the engine closure points; decide the two characterised behaviours |
| P10-02 | 3 / 3 | wire llama-bench report, tokenize output and fit-params line into their hosts |
| P10-05 | 3 / 3 | wire the release plan, build environment and artifact policy into scripts/tools |
| P10-08 | 3 / 3 | backend/gguf headers, SONAME/export compatibility and callback semantics |
| P10-10 | 3 / 3 | wire CPU features, platform paths and backend selection; non-Linux branches |
| P10-12 | 3 / 3 | close the 12 residual gaps (G1 tallies, G2/G3 prefill/trace, G6 moe-predict, ...) |

## What the canonical tree contains

`goose/` holds 891 canonical files outside `_archive` (4226 verified manifest
entries including the archive). The accepted slices of this phase cover:

- Jinja runtime 19025 lines (expressions, statements, filters, tests, macros,
  chat layer, streaming, delimiters, autoparser, tool builders, nine parser
  families) with differential gates;
- JSON/schema/PEG/GBNF: parser, dump, schema-to-GBNF converter, four tool gates;
- chat: parse params, thinking control, CLI line protocol, arena load;
- GGUF metadata context (writer, tensor data, split/merge still native);
- quantization type selection and allow-cuts (imatrix loader still native);
- HTTP URL policy, HF cache naming, download selection, log policy, console
  editing (P7-11 complete);
- server tool definitions and request schema, plus the MCP stdio integration
  (framing, pumps, reply matching, diagnostics, manager) closing P8-07 at 6/6;
- config validation (48 ordered rules), the MoE recipe registry and CLI
  resolution (flags > env > defaults);
- expert LRU, dense placement and row-stream planning;
- metrics/trace formatting and deterministic-workload validation;
- speculative decoding decisions (draft source, accept, MTP state, budget,
  rollback, trim);
- tokenizer type dispatch, special-token partition and common string helpers;
- meta backend, registry identity and threading/config acceptance; the ggml-opt
  optimizer closes P4-12 at 4/4;
- twenty model architecture builders over shared model-graph and family modules (P6-05 closed).

## Sources and evidence

- Sources: [`goose/`](../goose/README.md), grouped by subsystem.
- Map: `goose/MANIFEST.json` (experiment, native path, gate, SHA256 per file).
- Tools: `tools/sync-goose-tree.py` rebuilds the tree,
  `tools/check-goose-tree.py` verifies hashes and experiments (green).
- Every accepted slice has a card under `experiments/EXP-*` with the gate
  script, mutation table and recorded results.

## Composition debt

`tools/compose-goose.py` performs the deduplication: it resolves imports (the
parser is emitted once), drops identical declarations, applies per-module
renames for colliding helper families and refuses a set whose modules carry
different bodies under one name. It also fails when the composed unit does not
expose exactly the export set of its inputs. `tools/check-composition.sh` runs
the six accepted sets (server, quant, GGUF, P9-06, runtime+schema, app) through
`goose check`; all six currently pass.

What is left for one linked runtime: the Jinja copies (`builtin_call`/
`builtin_names`, `value_cmp`/`value_info`, `bases`, `type_name`, `as_string_into`)
are layout-specific variants rather than stale duplicates, so unifying them is a
real merge. The schema stub (`meta[3]=1`) still needs a linker seam because the
P7-08 gates build the runtime standalone.

## Open documentation debt

1. The root `README.md` still describes the pre-migration C++ runtime; it has a
   migration pointer at the top, but the results sections need a rewrite.
2. `docs/goose-migration/HANDOFF.md` is a historical narrative (EXP237-244), not
   the current state; this file replaces it as the status source.
3. 26 slice files have no native target in the ledger and sit under `misc/`.
4. Older experiments still track build artefacts committed before the storage
   rule; a cleanup needs Igor's explicit approval and must not delete sources.

## Next

1. Accept the in-flight module streams as they land: MCP stdio integration
   (P8-07 S5b), config/recipe registry (P9-01), expert streaming (P9-03),
   metrics (P9-07), backend and threading (P4-12), MTP (P6-11), loader and
   tokenizer (P7-12), and the composed-artifact smoke build.
2. P7-08: hoisted native names need a native insertion index per node; chat
   apply and time injection are the remaining chat-layer work.
3. Stage the accepted modules into the runtime in the order recorded in
   `RUNTIME-INTEGRATION.md` (S1 leaf modules, S2 server/CLI, S3 chat/Jinja, S4
   full runtime), each behind a build switch.
4. Keep `goose/` and this file current with every accepted slice.
