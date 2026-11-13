# FrankenMoE migration status

Updated: 2026-11-13. This file is the current state; update it in the same change
as every accepted slice.

## Ledger

`docs/goose-migration/tasks.json`: **DONE 102**, **PARTIAL 1** (P7-08),
**TODO 49** of 152 functional requirements. Slice front: **34 / 38** done.
Do not read the DONE count as project percentage; it counts requirements, not
code or time.

## Active requirement: P7-08 (Jinja, templates, reasoning)

Four of eight slices are complete:

| slice | experiment | state |
|---|---|---|
| lexer | EXP502 | done, 107 cases / 131025 values, 20/20 mutations |
| parser | EXP503 | done, 105 cases / 210 values, 19/19 mutations |
| value model | EXP504 | done, 271 cases / 271 values, 20/20 mutations |
| JSON output | EXP505 | done, 192 cases / 384 values, 20/20 mutations |
| built-ins | EXP506 | partial: scalar tables done, 51 cases, 14/14 mutations; string/array/object and global built-ins open |
| runtime | EXP507 / EXP508 | partial: expressions (81/162) and program/if/set (27/54, 12/12) done |
| runtime `for` | EXP509 | **in progress, not accepted**: port written, fixture and gate not finished |
| string/caps | — | not started |
| chat layer | — | not started |

The final `-O2` gate is deferred for EXP508 and EXP509: `gcc -O2` on the
generated module does not finish inside the five-minute per-step limit. ASan is
deferred for the same reason.

## What the runtime already uses

The C++ fork at `/home/end0/AI/LlamaServer/llama.cpp-flint-cuda` still provides
the transport; the Goose modules are compiled to `.so` and linked into it. The
accepted work covers CPU and CUDA operators, MoE, allocator, KV/batch/context,
server decisions, JSON/schema/PEG/GBNF and the Jinja stack.

## Sources and evidence

- Sources: [`goose/`](../goose/README.md), 4061 files grouped by subsystem.
- Map: `goose/MANIFEST.json` (experiment, native path, gate, SHA256 per file).
- Tools: `tools/sync-goose-tree.py` rebuilds the tree, `tools/check-goose-tree.py`
  verifies it (currently 4061 entries, green).
- Coverage: every `.goose` under `experiments/` and every LlamaServer checkout
  has a content-identical copy in `goose/` (6340 files checked, 0 missing).

## Open documentation debt

1. The root `README.md` still describes the pre-migration C++ runtime; it has a
   migration pointer at the top, but the results sections need a rewrite.
2. `docs/goose-migration/HANDOFF.md` is a historical narrative (EXP237-244), not
   the current state; this file replaces it as the status source.
3. 26 slice files have no native target in the ledger and sit under `misc/`.

## Next

1. Finish EXP509: fixture for `for`/`break`/`continue` and the loop object, gate
   and mutations, then update P7-08-COVERAGE and the ledger.
2. Continue P7-08 with macro/call, filters and tests, string/caps, then the chat
   layer.
3. Keep `goose/` and this file current with every accepted slice.
