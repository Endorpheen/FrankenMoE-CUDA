# P8-07 coverage — tools, MCP, structured output and decision helpers

Updated: 2026-11-14. Native sources: `tools/server/server-schema.cpp` (655
lines), `server-tools.cpp` (2172), `server-mcp.cpp` (799),
`server-decision.cpp` (1618).

The requirement owns five slices; S5 is an integration fixture, not a byte
gate. S1 is in acceptance; the rest is open.

| slice | native scope | state |
|---|---|---|
| S1. Request schema fields | `field*` helpers, aliases, hard limits, `make_llama_cmpl_schema` (76 fields), `make_tts_schema` | **done (EXP595)** |
| S2. Local file tools | path helpers, `path_glob_match`, `tools_io_basic`, the seven tool definitions and argument handling, `server_tools` dispatch | **done (EXP586)** — pure half only |
| S3. MCP session core | config parsing, `rpc_error_message`, `mcp_result_to_response`, the JSON-RPC session and manager over an abstract transport | in acceptance |
| S4. Decision core | `parse_questions`, `parse_state`, value transforms and renderers, options, variants/outputs, confidence, `format_answer`, `group_tasks` | open |
| S5. Integration shells | isolate/container/ssh IO, tool runtimes, stdio MCP transport, HTTP handlers, decision init/embeddings | open (integration fixture) |

## S3 plan (MCP session core)

Native anchors in `tools/server/server-mcp.cpp`: `parse_from_json` (136),
`parse_cursor_format` (140), `rpc_error_message` (182),
`mcp_result_to_response` (197), `send_rpc` (215), `ensure_init` (255),
`list_tools` (284), `call_tool` (316), and the manager at 639-799.

The port takes the transport as data: the session functions receive the next
request id, the JSON-RPC response text and the tool table, and return the next
request or the decoded result. The pwnd process transport (344-636) and the
stdio pumps stay in S5, so no process, thread or socket is involved.

The gate compares the request/response decisions against a transcription of the
native static functions and a fake in-process transport with canned replies:
config parsing (names, commands, args, env, cursor format), the
`MCP_PROTOCOL_VERSION` handshake, the error-message table, the
`result`/`content` unwrapping, tool listing and the call path (unknown server,
unknown tool, error result, success). Mutations must cover the version string,
the request id walk, the init-once rule, the tool-name lookup and the error
mapping.

## S4 plan (decision core)

Native anchors in `tools/server/server-decision.cpp`: `decision_question_type_name`
(10), `decision_meta_str` (26), `init` (36, model-dependent parts excluded),
`decision_is_payload` (216), `parse_questions` (220), `parse_state` (359),
`decision_replace_text` (420), `decision_sort_keys` (444), `decision_kev_render`
(467), `escape_special_tokens` (499), `decision_kev_text` (526),
`decision_d1omni_escape` (531), the Python-compatible string/float/space helpers
(557-718), `h2o_cut_state`/`h2o_quote` (725-748), `prepare_state` (774),
`render_h2o` (782), the D1-omni markers (827-836), `n_variants`/`n_outputs`
(838-846), the confidence functions and `format_answer`/`group_tasks`.

The slice is offline: questions and state are JSON documents, score vectors are
fabricated, and tokenization (`D1OMNI`, d1_labels) stays with D6. `init` from
model metadata, image/audio payload loading (`decision_load_image`/`audio`) and
`fill_task_laya` belong to S5.

The gate compares the parsed structures, the KEV/H2O/D1-omni rendered text, the
Python `dumps` output (indent, separators, ensure_ascii, float formatting) and
the confidence/answer formatting against a transcription with cited lines over
a fixed corpus and deterministic random documents. Mutations must cover the
renderer escapes, the option numbering, the cut/quote markers and the float
formatting.

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
