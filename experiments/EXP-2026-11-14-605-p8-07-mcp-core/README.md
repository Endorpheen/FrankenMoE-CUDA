# EXP-2026-11-14-605 — MCP session core (P8-07 S3)

Native counterpart: `tools/server/server-mcp.cpp` (`parse_cursor_format` 140,
`rpc_error_message` 182, `mcp_result_to_response` 197, `ensure_init` 255,
`list_tools` 284, `call_tool` 316). The stdio transport (344-636) and the
manager (639-799) are S5. Base commit: `05048b02`.

## What this slice covers

`source/mcp-core.goose` takes the transport as data — no process, thread or
socket:

| export | native | what it returns |
|---|---|---|
| `mcp_config_lines` | `parse_cursor_format` | one line per server in byte-sorted name order: `name|command|cwd|timeout|args|env`; entries without a command are skipped, `timeout_ms` defaults to 30000 |
| `mcp_rpc_error` | `rpc_error_message` | `error.message` for an object, the string for a string, `unknown error` otherwise |
| `mcp_result_response` | `mcp_result_to_response` | text parts joined with newlines; `isError` becomes `{"error": ...}` with the default message for empty text, otherwise `{"plain_text_response": ...}` |
| `mcp_init_request` / `mcp_initialized_notification` / `mcp_list_tools_request` / `mcp_call_tool_request` | `ensure_init`, `list_tools`, `call_tool` | the exact JSON-RPC requests, in the library's key order |
| `mcp_parse_tools` | `list_tools` | one line per tool: `name|description|schema` where schema is `y`/`n` |
| `mcp_rpc_verdict` | `send_rpc` | 0 for a matching reply, 1 for an error object; the order is write failure, no id, match, cancel, deadline, closed |

The parser is the accepted one (`jsonparse`, a byte-identical copy of
`goose/json/json-parser.goose`).

## Gate

`tests/build.sh` builds the module at `-O0` and links `tests/fixture.cpp`,
which carries a native transcription of the same functions on nlohmann::json
(same library the server uses). 26 cases compare the config lines, the error
and result mappings, all four request builders, the tool list and six verdict
rows.

```
FG_MCP cases=26 failures=0
```

## Mutations

`tests/mutate.py 0 6|6 12` holds 12 semantic mutations in two halves:

```
python3 tests/mutate.py 0 6    # 6/6 caught
python3 tests/mutate.py 6 12   # 6/6 caught
```

## Recorded limits

- The stdio transport (`server_mcp_stdio`), the process pumps and the manager
  (start/get_or_create/cooldown) stay in S5; they need a real process.
- `send_rpc` is modelled as a verdict over (written, has_id, match, cancel,
  deadline); the actual framed read loop is not ported.
- Tool `inputSchema` is reported as presence only, not re-serialized.
- The call request takes canonical arguments JSON from the caller; the fixture
  passes byte-sorted arguments so the dump matches.
