# FrankenMoE migration status

Updated: 2026-11-19 (25). This file is the current state; update it in the same change
as every accepted slice.

## Ledger

`docs/goose-migration/tasks.json`: **DONE 123**, **PARTIAL 19**, **TODO 10** of 152 functional requirements.
Do not read the DONE count as project percentage; it counts requirements, not code or time.

Recounted from the records themselves on 2026-11-19; this file said 121 / 21 / 10 and was two
requirements behind.

### PARTIAL

| requirement | what is open |
|---|---|
| P6-10 | grammar reject port: unit gate green (tests/grammar/reject-alternates 7 cases, first-set 6 cases) and the defect fixed in Goose, but the real-vocabula... |
| P6-11 | MTP/draft-model routes for synth and rejection acceptance (needs a draft GGUF matching the target vocabulary); eleven decisions not yet wired to the h... |
| P7-06 | delete the replaced C++ tokenizer body - blocked by rule 15 until Igor accepts the served encoder |
| P7-07 | delete the replaced C++ detokenizer body - blocked by rule 15 until Igor accepts the served forms |
| P7-12 | 1) delete the replaced native GGUF/reader/tokenizer bodies - blocked by rule 15 pending acceptance (the wiring itself is done and measured: FG_GGUF_VA... |
| P8-06 | switch the reload sets and the connect/source choice, then exercise the lifecycle switches on a running model (load, stop, unload, remove) which needs... |
| P8-09 | wire web_serve and the API-key middleware into server-http.cpp |
| P8-12 | remove the remaining native server bodies after each switch is accepted: the Origin three-compare arm and the find_tool linear scan are the last two t... |
| P9-03 | replace the 25 call sites in the expert cache, dense placement and row streaming with the Goose decisions (commit-before-touch, budget return, caller-... |
| P9-04 | rewrite on_eval to the rh_topk_plan/rh_ask_plan contract and replace the overlap/row-source call sites |
| P9-05 | extract expert_bytes_per_slot(meta), keep rts_config as a drift pin, and wire rts_n_batch/rts_ubatch plus the MTP head admission order |
| P9-07 | wire the CSV, decode-trace and route-trace sinks and DeterministicWorkload to the Goose rows (integration step) |
| P9-08 | wire the closure decisions into session.cpp and cli/main.cpp |
| P10-06 | build-time CUDA architecture and flag decisions: CMake picks CMAKE_CUDA_ARCHITECTURES itself (vendor/flint/ggml/src/ggml-cuda/CMakeLists.txt), and a c... |
| P10-07 | write the adapter from the parallel arrays into the common_json pimpl, shadow-compare it against nlohmann at common/json.cpp:205 using the text the du... |
| P10-10 | Apple/Windows branches are transcription only on this host |
| P11-01 | standalone runtime has no CUDA, tokenizer engine or HTTP (the working Apex path is the INTEGRATED stage-run, not the standalone - rule 15 forbids repo... |
| P11-04 | D1: F2 gguf_write_to_buf, F3 gguf_get_meta_size and F4 gguf_get_meta_data (all native in vendor/flint/ggml/src/gguf.cpp, reached only by llama-quant.c... |
| C1-02 | T_NEXTN_HC_HEAD ids exist only in the integration tree, not the pinned upstream |

### TODO

| requirement | what is open |
|---|---|
| P11-07 | blocked on the 19 open APEX_COMMON tasks listed by tools/apex-accounting.py: P6-10, P7-06, P7-07, P7-12, P8-06, P8-09, P8-12, P9-03, P9-04, P9-05, P9-... |
| C1-03 | Qwen quantized precision, cold-bank and expert hook differences |
| C1-04 | shared loader, tokenizer and memory interfaces needed by Qwen |
| C1-05 | complete Qwen inference qualification in the common artifact |
| C1-06 | Apex protection while Qwen support is extended |
| C2-04 | remaining architecture extensions (Qwen builders, precision, loader and qualification) without regressing the common runtime or Apex |
| F1-01 | All workstreams implemented; no owned handwritten C/C++/CUDA execution hidden as bridge/vendor dependency; historical controls excluded from candidate... |
| F1-02 | All required functional tasks have evidence; real Apex/Qwen and representative other family fixtures; one common artifact and reproducible rollback. |
| F1-03 | Every remaining dependency justified with source/symbol/version/license; replaceable owned parser,kernel,scheduler or server is not called unavoidable... |
| F1-04 | Every functional task DONE; zero untracked residual native paths; source build reproducible and working runtime unchanged. Then comprehensive audits/o... |


|---|---|---|
| P6-11 | 3 / 3 | wire ngram_draft.cpp and session.cpp to the Goose decisions |
| P7-12 | 3 / 3 | wire the tokenizer dispatch, special-token partition and common strings |
| P8-06 | 3 / 3 | wire model_router.cpp to mg_/hr_/cn_ |
| P9-01 | 3 / 3 | wire validate(), arch_registry.cpp and main.cpp |
| P9-03 | 3 / 3 | replace the 25 streaming call sites |
| P9-04 | 3 / 3 | rewrite on_eval to the rh_ plan and replace the overlap/row-source call sites |
| P9-05 | 3 / 3 | extract expert_bytes_per_slot and wire rts_n_batch/rts_ubatch |
| P9-07 | 3 / 3 | wire the three sinks and DeterministicWorkload |
| P9-08 | 10 / 10 | wire the closure points; decide the two characterised behaviours |
| P10-02 | 3 / 3 | wire llama-bench report, tokenize output and fit-params line into their hosts |
| P10-05 | 3 / 3 | wire the release plan, build environment and artifact policy into scripts/tools |
| P10-08 | 6 / 6 | device backend headers, ggml-cpp.h and the scheduler-eval runtime sequence |
| P10-10 | 3 / 3 | wire CPU features, platform paths and backend selection; non-Linux branches |
| P10-12 | 3 / 3 | close the 8 remaining audit gaps (G4 MTP warm-up, G5 sampler chain, G7 CLI driver, ...) |
| P10-07 | 3 / 3 | replace or plan replacement of the 9 replaceable vendored components |
| P10-09 | 3 / 3 | wire the runner, fixture and diagnostics decisions into tests/tools |
| P8-09 | 6 / 6 | wire web_serve and the API-key middleware into server-http.cpp; live server run needs approval |
| P11-01 | — | IQ tables at init; switch the remaining layers; run the Apex A/B once approved |
| P1-12 | — | the four single-block dots are Goose-owned behind a runtime switch (EXP667); a multi-block row needs a row-level SIMD entry point (EXP668) |

## What the canonical tree contains

`goose/` holds 931 canonical files outside `_archive` (4266 verified manifest
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
  the five P8-12 server call sites now ask the composed unit first by default
  (model status, Origin host, asset content type, Cache-Control, tool dispatch),
  three of them with the native body deleted (EXP661, EXP663);
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
- Integration layer: `integration/m1/` holds the eleven carrier patches, the
  eight stage bridges, the host adapter and the twelve gates; the M1 project
  there builds both configurations. `tools/stage-m1-tree.sh` assembles the whole
  staged tree from `upstream/llama.cpp` + `integration/m1` + `goose/` (EXP664),
  so the staged tree is now a function of tracked inputs instead of a `/tmp`
  copy. Generated C is not committed: `--with-unit` composes it.
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
