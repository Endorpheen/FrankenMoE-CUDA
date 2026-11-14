# REPORT — P8-07 S5b: MCP stdio integration over a real process

Base commit: `29a930e6`. Working copy: `EXP-2026-11-15-619-p8-07-mcp-integration`
(scratch `/tmp/opencode/sub-619`), not committed.

## Result

The integration half of `server_mcp` runs: a fake MCP server is spawned as a
separate process, spoken to over NDJSON pipes, initialised, listed and called,
and every protocol decision on the way is Goose code composed with the four
accepted server modules into one unit.

- gate: 26 end-to-end steps, 243 cases, 0 failures
  (`FG_MCPINT steps=26 cases=243 failures=0`);
- mutations: 12/12 caught, two halves of six (`0 6` and `6 12`);
- unit: 7 modules composed (`goose check` ok), one binary, 1.49 MB of generated
  C and not part of the experiment;
- no model, no GPU, no network. Wall time of the whole gate: 3 s.

## What was compared

Each scenario runs twice, against two separate instances of the same fake
server: once through a transcription of `tools/server/server-mcp.cpp` on
`nlohmann::json`, once through the Goose decisions. Then compared byte for byte:
the exact framed bytes sent, the exact lines received, the tool table, the call
response, `last_error` and `diagnostics`; plus the scenario's goldens on the
port side (transcript, response, error, diagnostics, tool table, drop count).

| group | steps | native anchor |
|---|---|---|
| discovery and call, success paths | 1, 2, 7, 22 | `ensure_init` 255, `list_tools` 284, `call_tool` 316 |
| error mapping | 3, 4, 5, 6 | `rpc_error_message` 182, `mcp_result_to_response` 197 |
| timeout | 8, 9 | `send_rpc` 215-253 |
| dead and killed server, partial stream | 10, 11, 19 | `send_rpc` 243-252, `mcp_pump_ndjson` 24 |
| frame-level faults | 12, 13, 14, 15, 16, 17, 18 | `mcp_pump_ndjson` 24-129 |
| config surface | 20, 21 | `mcp_build_env` 446, `start` 693 |
| stderr tail and diagnostics | 23 | `errlog_loop` 618, `diagnostics` 521 |
| manager and cooldown | 24, 25 | `get_or_create` 763-819 |
| one-unit reachability | 26 | `tools-pure`, `server-schema`, `decision-core` |

The native side is a transcription on `std::string`, `poll`, `nlohmann::json`
and real pipes with the source line at every function, not a re-implementation
of the port: the port's decisions are compared against it, not against
themselves. The golden transcript of step 1 pins the exact four request frames
of the native order, so the comparison does not rest on the transcription alone.

## Mutations

| # | mutation | caught by |
|---|---|---|
| 0 | the frame ends with CRLF | `success/sent` |
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

Two decisions were repaired because a mutation exposed them during this work:
the gate first recorded payloads instead of frames (mutation 0 was invisible)
and the oversized-line fault was split across two writes that the poll
granularity merged (mutation 2 was invisible). Both are now real: the transcript
carries framed bytes and the fake server delivers the newline together with the
reply.

An earlier draft used the accepted `ms_err_tail` for the live stderr drain; the
model keeps the first `cap` bytes once the total exceeds it, while `errlog_loop`
keeps the last. The slice now owns `mi_err_tail` (keep the last `cap` bytes) and
the 6.6 KB case is compared against the transcription.

## What stays out

- The three pump threads, the reply queue bound, the mutexes and the thread
  joins: the gate's host reads on demand in one thread. The queue contract is
  unchanged, not exercised.
- HTTP handlers (`/tools`), the isolate/container/ssh tool runtimes, the
  decision `init` and embeddings, `fill_task_laya`: other S5 parts, untouched.
- Windows transports: `SearchPathW`, `PIPE_NOWAIT`, `CreateProcess`, the
  `PIPE_NOWAIT` sleep. Only the POSIX `poll` path exists in the transcription.
- The warmup budget as a race: the per-request timeout (150 ms / 4 s) is far
  below the 10 s budget, and `should_stop` is false on both sides.
- Frame parsing after acceptance and the `result` extraction are the host's
  `nlohmann` marshalling, as native `resp.at("result")` is; the branch decision
  and every string come from Goose.
- S5b remains an integration fixture and is marked as such in the coverage
  document: no byte parity is claimed for a slice that spawns processes.

## Integration path

The composed unit is what a single Goose runtime would link. In place:

1. keep `fork`/`exec`, the pipes, the three threads in `server_mcp_stdio`;
2. `writer_loop` -> `mi_frame`, `reader_loop` -> `mi_lines`, `errlog_loop` ->
   `mi_err_tail`;
3. `send_rpc` keeps its queue and deadline; id matching -> `mi_match`, the three
   exits -> `mi_stop_text`, the deadline comparison -> `mi_deadline_hit`;
4. `ensure_init`/`list_tools`/`call_tool` -> `mi_next_method`, `mi_next_id`,
   `mi_call_kind`;
5. `diagnostics()` -> `mi_diag`;
6. `get_or_create` state -> `ms_manager_resolve` action codes.

Steps 2-5 are what the 26 steps above exercise against a live process; step 6 is
exercised as data (nine states) and once through a real failed spawn.

## Hashes (SHA-256)

| file | SHA-256 |
|---|---|
| `source/mcp-integration.goose` | `5e690752d7941802d76fa9500596606bc0cb67dd112a9f770506f4c7a366c174` |
| `source/mcp-core.goose` (canonical copy) | `477577fc33092f06cfa9b7bb69a4c4f5d65c90b03ebb26a8dce00632a3fd2d8c` |
| `source/mcp-stdio.goose` (canonical copy) | `99b9bcc3e555363c777b4da2334ec75783d2baeb998caeb67ecd108e80f34c92` |
| `source/decision-core.goose` (canonical copy) | `3bc31f59a98b05954ef2041335a6522b2b47dfc07b391d065c0fd5d10283450b` |
| `source/server-schema.goose` (canonical copy) | `40a367df5a1d661e1e6a577c3b57150a3dc08354721c94b832034e330ef531af` |
| `source/tools-pure.goose` (canonical copy) | `d42b37fac1a3201b36eaf3b4158caf427860144c524502f61e101993e9f61903` |
| `tests/fixture.cpp` | `3b5605dfd37b1c2a317451c2257661107ebc283a7cd39fec9f20f4b2bbb24f42` |
| `tests/fake_server.cpp` | `6acbf73813242924174367562533a1ae388268b3a5e31c17a10c09d620eb6c10` |
| `tests/build.sh` | `cf7f98ed4877a61cbfd12a004a8cafdbd027573d8dc43289b782588b60e506ee` |
| `tests/mutate.py` | `b150762a2cfe5e50bed61fa8f68f004f34e301cea9ffb904e8a861abf0c31f3b` |
| `tests/gate.log` | `8e9e055037d542c9b5a1b7877d406124a7fed55b03ed50cd785bc70adc104dac` |

The five canonical copies are byte-identical to `goose/server/` at `29a930e6`
(`cmp` clean for all five).

Toolchain: goose 2026.3-string, gcc-13/g++-13 at `-O1` for the gate and `-O0`
for the mutants, `nlohmann/json` from `llama.cpp-flint-cuda/vendor`.
