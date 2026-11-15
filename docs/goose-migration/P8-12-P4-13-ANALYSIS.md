# P8-12 / P4-13 — server removal and lineage unification (analysis)

Status: **analysis, not started**. Both requirements are TODO with no slices; this
note records what exists and the order in which it can be done.

## P8-12 — remove the native server implementation from the candidate executable

Already done:

- `runtime/reconstructed/server-merge/` holds one merged `server-context.cpp`
  containing all four ports (`fg268p`, `fg272s`, `fg274b`, `fg276t`; 29
  references, 158 changed lines vs upstream) and `server-queue.cpp`.
- `link_server.py` compiles the merged file with the recorded command, replaces
  `server-context.cpp.o` inside a copy of `libserver-context.a`, relinks
  `libllama-server-impl.so` and appends the four bridge objects.
  Result: compile rc=0, link rc=0, library 7 624 752 bytes, 19 defined and 0
  undefined `fg*` symbols.
- The server decision modules are accepted separately: MCP core/stdio/integration,
  request schema, decision core, tools, model manager, hot reload, connectors,
  web assets, MIME, ETag, route table, log policy, console editing, HF cache and
  download selection.

Not done:

1. The shipped `llama-server` is not built from the merged copies and the
   application binary is not relinked against them.
2. The S2.3 server/CLI call sites (49 files / 18 318 lines) are not switched to
   the Goose decisions.
3. The removal order must follow the switch order: request path, tools, MCP,
   model management, web assets, then the remaining native server files.

Prerequisite: a staged build that links `llama-server` with all server flags ON
and no model run (the P11-01 M1 increment), then the per-stage A/B protocol in
`APEX-TEST-PLAN.md`.

## P4-13 — unify allocation and scheduler interfaces across both lineages

Already done:

- P4-05/06/07 closed the allocator and backend-interface criteria
  (`docs/goose-migration/P4-ALLOC-BACKEND-CLOSURE.md`); EXP161/162/163 carry the
  gates.
- P4-12 closed the meta backend, registry identity, threading and the ggml-opt
  optimizer (EXP617/EXP624).
- P9-03/P9-04 ported expert LRU, dense placement, row-stream planning, router
  hooks and overlap rules.

Not done:

- The two lineages still present different allocation and scheduler entry
  points; a single interface needs a decision on which side owns the block
  tables and the eviction callbacks.
- The unification must keep the accepted gates green and cannot change the Apex
  hot path without an A/B.

Order: document the two interfaces, pick the owner, then port the adapter with a
differential gate before any call site is switched.
