# MCP stdio integration (P8-07 S5b)

The integration half of `server_mcp`: a real child process spoken to over real
pipes, with every protocol decision taken by one composed Goose unit. Base
commit `29a930e6`.

The unit is built with `tools/compose-goose.py` from the accepted server modules
(`mcp-core`, `mcp-stdio`, `decision-core`, `server-schema`, `tools-pure`) plus
this slice's `mcp-integration.goose`:

```
composed 7 module(s) -> build/unit.goose   (goose check: ok)
```

`tools/check-composition.sh` already asserts that the five accepted modules
share one unit (`ok server`); this experiment adds the glue to that set and
links it into a gate.

This is an integration fixture, as the coverage document requires: it is never
byte parity. It shows that the accepted decisions survive contact with a live
process, and it compares behaviour against a transcription of the native host
code driven over a second live process.

## What the host owns and what Goose owns

The host is `tests/fixture.cpp`: `fork`/`exec` with a close-on-exec status pipe
(the `sp.create()` contract), the three pipe pairs, poll loops, `waitpid`, the
monotonic clock, and `nlohmann::json` for reading a frame that the port already
accepted as a reply.

Everything a decision consists of stays in Goose:

| export | native anchor | decision |
|---|---|---|
| `mi_frame` | `mcp_write_all` 537, `writer_loop` 604 | one NDJSON frame: payload plus `\n`; a payload with an embedded newline is refused |
| `mi_lines` | `mcp_pump_ndjson` 24 | incremental pump over live poll chunks: split on `\n`, strip one `\r`, skip empty lines, drop an unterminated tail at EOF, drop an oversized unterminated line and resynchronise at its newline |
| `mi_match` | `send_rpc` 235 | 1 = deliver: an id-less request takes the first frame, a matching id is a reply; 0 = skip: malformed frame, notification, stale id |
| `mi_stop_text` | `send_rpc` 243 | post-loop verdict text: cancel before deadline, deadline before a closed transport |
| `mi_deadline_hit` | `send_rpc` 220 | `elapsed >= timeout` is the deadline |
| `mi_next_method` | `ensure_init` 255, `list_tools` 284, `call_tool` 316 | the session order: initialize, notifications/initialized, tools/list, tools/call |
| `mi_next_id` | `next_id` (server-mcp.h) | the id walk, reset per spawn |
| `mi_call_kind` | `call_tool` 316 | error object / result / neither, in that order |
| `mi_err_tail` | `errlog_loop` 618 | append a stderr line and keep the last `cap` bytes |
| `mi_diag` | `diagnostics` 521 | `last_error` then `; last stderr: <tail>` |

Reused from the accepted modules, unmodified:

| export | module | used for |
|---|---|---|
| `mcp_init_request`, `mcp_initialized_notification`, `mcp_list_tools_request`, `mcp_call_tool_request` | mcp-core | request bytes, `nlohmann` key order |
| `mcp_rpc_error`, `mcp_result_response`, `mcp_parse_tools` | mcp-core | error text, `/tools` normalisation, the tool table |
| `ms_resolve_candidates` | mcp-stdio | spawn candidate order |
| `ms_env_build` | mcp-stdio | child environment |
| `ms_manager_resolve` | mcp-stdio | create/cooldown policy |
| `ms_limits` | mcp-stdio | cooldown 5 s, warmup 10 s, poll 50 ms, line cap, tail cap |

`decision-core`, `server-schema` and `tools-pure` are in the unit because the
runtime is one translation unit; the gate asserts they are reachable from the
same binary (`tool_name(0)`, `schema_row_count()`, `dc_py_float_of`, `ms_limits`)
but does not exercise their behaviour, which their own gates own.

## The fake server

`tests/fake_server.cpp` is a separate process: NDJSON on stdin, NDJSON on
stdout, diagnostics on stderr. It implements `initialize`,
`notifications/initialized`, `tools/list` and `tools/call` with six tools
(`echo`, `add`, `fail`, `fail_no_text`, `env`, `argv`) and refuses
`tools/list`/`tools/call` until the notification arrives, like a real server.

`argv[1]` is a `:`-separated flag list that injects one fault: `slow`, `late`,
`silent`, `init_error`, `empty_list`, `junk`, `bigsplit`, `blank`, `notify`,
`stale`, `crlf`, `chunked`, `die_after_init`, `eof_partial`, `stderr`,
`stderr_long`, `bigdesc`.

## Gate

```
bash tests/build.sh
```

The gate runs every scenario twice: once through the native transcription, once
through the Goose decisions, each against its own fake server process. The two
transcripts (exact framed bytes sent, exact lines received), the tool table, the
call response, `last_error` and `diagnostics` must be identical, and the
scenario's documented golden values are asserted on the port side.

```
FG_MCPINT steps=26 cases=243 failures=0
```

| # | step | what it proves |
|---|---|---|
| 1 | `success` | full exchange; golden transcript of all four frames, 6 tools, `pong` |
| 2 | `multi_text` | two text parts joined by `\n` |
| 3 | `iserror` | `isError` maps to `{"error":"<text>"}` |
| 4 | `iserror_empty` | `isError` without text maps to the fixed message |
| 5 | `unknown_tool` | server error object surfaces its message |
| 6 | `init_error` | failed initialize stops the session, message prefixed |
| 7 | `empty_list` | a server with no tools is not an error |
| 8 | `timeout_init` | a slow initialize hits the deadline |
| 9 | `timeout_list` | a slow tools/list hits the deadline |
| 10 | `killed_before_call` | `SIGKILL` after discovery: call fails with `transport closed`, diagnostics carry the stderr tail |
| 11 | `die_after_notify` | the server exits during discovery: EOF is `transport closed`, not a timeout |
| 12 | `malformed_then_ok` | a junk frame is skipped, the real reply is delivered |
| 13 | `notification_first` | a server notification before the reply is skipped |
| 14 | `stale_id` | a reply with a foreign id is skipped |
| 15 | `crlf` | CRLF framing is normalised |
| 16 | `chunked_reply` | a reply written one byte at a time is reassembled |
| 17 | `oversize_resync` | an unterminated line over the cap is dropped, the stream resynchronises at its newline (drops=1 on both sides) |
| 18 | `blank_lines` | empty and CR-only lines are skipped |
| 19 | `eof_partial` | a partial line at EOF is dropped, not parsed |
| 20 | `env_override` | the config environment reaches the child (`MCP_FAKE_TAG`) |
| 21 | `argv_passthrough` | config args reach the child |
| 22 | `big_description` | a 2 KB description survives the tool table |
| 23 | `stderr_tail` | a 6.6 KB stderr stream is bounded to the last 4096 bytes |
| 24 | `spawn_failure` | a missing command fails the spawn on both sides, no frame is sent, the manager records the cooldown and blocks the retry |
| 25 | `manager_rows` | nine `get_or_create` states against the native transcription |
| 26 | `unit_symbols` | the other three accepted modules are in the same binary |

Cases per scenario: spawned, frames sent, lines received, tool table, call
response, `last_error`, diagnostics, oversized-line drops; plus the golden
values where the scenario fixes them (transcript, response, error, diagnostics,
tool table, drop count).

## Mutations

```
python3 tests/mutate.py 0 6     # 6/6 caught
python3 tests/mutate.py 6 12    # 6/6 caught
```

Twelve semantic edits in two halves; each one rewrites a decision, rebuilds at
`-O0` and must make the gate fail. A mutation that breaks the build counts as
caught.

| # | mutation | caught by |
|---|---|---|
| 0 | the frame ends with CRLF instead of LF | `success/sent` |
| 1 | a pump line keeps its carriage return | `crlf/got` |
| 2 | the resync drops the rest of the buffer | `oversize_resync/got` |
| 3 | a notification frame is taken as the reply | `notification_first/tools` |
| 4 | the initialized notification is never sent | `success/sent` |
| 5 | any numeric id matches the request | `timeout_init/sent` |
| 6 | a timeout is reported as a closed transport | `timeout_init/final` |
| 7 | an error reply is taken as a result | `unknown_tool/final` |
| 8 | diagnostics drop the stderr tail | `killed_before_call/diag` |
| 9 | the stderr tail keeps the first bytes | `stderr_tail/diag` |
| 10 | request ids never advance | `success/sent` |
| 11 | a failed start records no cooldown | `mgr_row6` |

`mutate.py` restores every source after each mutation and rebuilds at the end; a
mutation that changes nothing observable is reported as `MISSED`, so a silently
dead decision would show up here.

## Recorded limits

- The host, not Goose, holds the process, the descriptors, the clock and the
  thread. The native transport runs three pump threads and a bounded reply queue
  (`from_server.max_size = 65536`); this gate reads on demand in one thread and
  keeps a `std::deque`, so the queue bound, the mutexes and the thread joins are
  not compared. Teardown is `close` + `waitpid` in the gate.
- A complete frame is parsed with `nlohmann::json` on both sides after the port
  has accepted it; extracting the `result` object for `mcp_result_response` is
  the host's marshalling, exactly what `resp.at("result")` does natively. The
  reply branch decision itself (`mi_call_kind`) and every error string come from
  Goose.
- `mi_lines` truncates a chunk that does not fit its buffer (64 KiB in the
  gate), the same contract as S5a's model; the native `std::string` grows.
- The native `should_stop` in these paths is the warmup budget (10 s) or a
  shutdown flag; the gate keeps it false and uses the per-request timeout
  (150 ms for the timeout scenarios, 4 s otherwise, 8 s for the byte-at-a-time
  reply), so the warmup race is not compared.
- `mcp_resolve_command` on POSIX returns the command unchanged and `sp.create`
  searches `PATH`; the transcription therefore uses `execvp`/`execvpe`, while
  the port resolves candidates with `ms_resolve_candidates` and the host stats
  them. The directory list is passed with a trailing `/` because the port keeps
  the native Windows join rule (it already accepts both separators). The
  Windows branches (`SearchPathW`, `PIPE_NOWAIT`, `CreateProcess`) are not
  modelled.
- The golden `timeout` numbers are wall clock, so a mutation can shift which
  frame a scenario fails on; the gate fails either way.
- Windows-only behaviour, the HTTP handlers, the container/ssh tool runtimes,
  the isolate backend, the decision `init`/embeddings, `fill_task_laya` and the
  log lines (`SRV_DBG`/`SRV_WRN`) are outside this slice.
- The composed unit is a 1.49 MB generated C file, above the 1 MiB
  generated-file limit, so `build/` is never part of the experiment.

## Files

| path | what |
|---|---|
| `source/mcp-integration.goose` | this slice's ten decisions |
| `source/{mcp-core,mcp-stdio,decision-core,server-schema,tools-pure}.goose` | byte-identical copies of the accepted modules (SHA-256 compared with `goose/server/`) |
| `tests/fixture.cpp` | the host, the native transcription, the scenarios and the goldens |
| `tests/fake_server.cpp` | the fake MCP server process |
| `tests/build.sh` | compose, build at `-O1`, run; `FG_OPT=-O0` for mutants |
| `tests/mutate.py` | the twelve mutations, `mutate.py [lo hi]` |
| `tests/gate.log` | the recorded gate output |
| `REPORT.md` | result, table, hashes |

## Integration path

The unit is ready to replace the native transport in place:

1. compose the unit as `tests/build.sh` does and link it into the server;
2. `server_mcp_stdio::start` keeps `fork`/`exec`, the pipes and the three
   threads; the writer takes `mi_frame` instead of appending `'\n'` itself, the
   reader takes `mi_lines` instead of calling `mcp_pump_ndjson`, and
   `errlog_loop` takes `mi_err_tail`;
3. `send_rpc` keeps the queue and the deadline; the id check becomes
   `mi_match`, and the three return paths become `mi_stop_text`;
4. `ensure_init` / `list_tools` / `call_tool` take their method from
   `mi_next_method`, their id from `mi_next_id`, and their branch from
   `mi_call_kind`;
5. `diagnostics()` becomes `mi_diag` over the tail that `mi_err_tail` builds;
6. `get_or_create` keeps its mutex and maps its state onto
   `ms_manager_resolve`'s action code.

The gate above is the compatibility check for steps 2-5 over a live process.
