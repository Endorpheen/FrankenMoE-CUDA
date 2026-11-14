# P8-07 coverage — tools, MCP, structured output and decision helpers

Updated: 2026-11-14. Native sources: `tools/server/server-schema.cpp` (655
lines), `server-tools.cpp` (2172), `server-mcp.cpp` (799),
`server-decision.cpp` (1618).

The requirement owns five slices; S5 is an integration fixture, not a byte
gate. S1 is in acceptance; the rest is open.

| slice | native scope | state |
|---|---|---|
| S1. Request schema fields | `field*` helpers, aliases, hard limits, `make_llama_cmpl_schema` (76 fields), `make_tts_schema` | in acceptance |
| S2. Local file tools | path helpers, `path_glob_match`, `tools_io_basic`, the seven tool definitions and argument handling, `server_tools` dispatch | **done (EXP586)** — pure half only |
| S3. MCP session core | config parsing, `rpc_error_message`, `mcp_result_to_response`, the JSON-RPC session and manager over an abstract transport | open |
| S4. Decision core | `parse_questions`, `parse_state`, value transforms and renderers, options, variants/outputs, confidence, `format_answer`, `group_tasks` | open |
| S5. Integration shells | isolate/container/ssh IO, tool runtimes, stdio MCP transport, HTTP handlers, decision init/embeddings | open (integration fixture) |

## Boundaries and dependencies

- S1 excludes `json_schema_to_grammar` (already canonical) and the `tools` /
  `tool_choice` fields, which belong to P8-04.
- S1 needs the sampling handles (D5), JSON (D1) and, for preserved tokens and
  grammar triggers, `common_sampling_add_*` (D3) and a tokenizer (D6). The
  offline oracle covers the field table and validation; tokenizing preserved
  strings stays native.
- S2 excludes the isolate/container/ssh backends and the runtimes (S5).
- S3 excludes the stdio process transport (S5); the session runs over a fake
  in-process transport.
- S4 excludes model-dependent `init` and `fill_task_laya` (S5).
- S5 cannot be proven offline: acceptance must be an integration fixture with
  stubs.

## Oracle policy

Every slice up to S4 must use an offline differential probe (no network, no
model weights) with one generator emitting both the native probe and the Goose
driver, plus mutations. S5 is accepted by an integration fixture and is marked
as such, never as byte parity.
