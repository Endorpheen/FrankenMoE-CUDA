# MCP stdio transport shells (P8-07 S5, part)

The offline half of the `server_mcp_stdio` transport and the `server_mcp`
manager: `resolve_command`, the environment merge, the framed write loop, the
reader/errlog pump decisions, the stderr tail, the manager's create/cooldown
policy and the startup warmup budget. Base commit `b68cffed`.

The native file is `tools/server/server-mcp.cpp`. No process is spawned and no
thread runs here: the transport is data. The spawn itself, the real pipes, the
three pump threads, the HTTP handlers, the tool runtimes and the decision
init/embeddings stay an S5 integration fixture.

## What this slice covers

`source/mcp-stdio.goose` (prefix `ms_`).

| export | native anchor | what it returns |
|---|---|---|
| `ms_resolve_candidates` | `mcp_resolve_command` 379 | the ordered spawn candidates, one per line: exact name in every search directory, then one PATHEXT extension at a time across every directory; a command with a separator is searched only as a path |
| `ms_env_build` | `mcp_build_env` 446 | the child environment: parent entries whose key is not overridden, verbatim and in order, then the overrides sorted by key as `KEY=VALUE` |
| `ms_write_plan` | `mcp_write_all` 537 | the write loop verdict over a scripted result sequence: `rc`, bytes written, loop steps, polls, poll timeouts |
| `ms_pump_lines` | `mcp_pump_ndjson` 24 | the NDJSON pump over scripted pipe chunks: final state, oversized-line drops and the emitted lines, with CR stripped |
| `ms_err_tail` | `errlog_loop` 618 | the bounded stderr tail after the lines are appended |
| `ms_manager_resolve` | `get_or_create` 771 | the transport decision: action, the old/new transport closes and the new cooldown deadline |
| `ms_warmup_plan` | `server_mcp::start` 693 | per config: duplicate skip, spawn failure, `spawn+10 s` deadline, aborted discovery and the accumulated registry |
| `ms_limits` | 657-658, 31-32, 619, 676 | the constants as data: cooldown, warmup, poll interval, line cap, err tail, transport kind |

Owned decisions:

- the extension rule: a name with an extension is not extended again, and a dot
  in a directory component does not count as an extension; a path-shaped
  command never falls back to the search path; an empty PATHEXT uses
  `.COM;.EXE;.BAT;.CMD`; empty `;;` tokens produce no candidate; a trailing
  separator in a directory is not doubled; when nothing exists the command is
  returned unchanged so the spawn reports the error;
- the environment merge: split at the first `=`, an entry without `=` has the
  whole text as its key, a matching override removes the parent entry,
  duplicates of the parent are all kept or all removed, the overrides are sorted
  by key because the native container is `std::map`;
- the write loop: append the newline frame to the message, a zero return fails,
  `EINTR` retries, `EAGAIN` polls and a timeout retries, `POLLHUP`/`POLLERR`/
  `POLLNVAL` fail, a cleared `running` stops the loop and returns
  `total == size`, so a partial write is a failure;
- the pump: split on `\n` only, strip one `\r`, skip empty lines, a partial line
  at EOF is dropped, a consumer that refuses a line stops the pump after
  delivering that line, `POLLHUP` is not fatal on the read path while it is on
  the write path, `EINTR`/`EAGAIN`/timeouts retry, and an over-long unterminated
  line is dropped and the stream resynchronised at the next `\n`;
- the stderr tail: each line is appended with a newline and the tail keeps the
  last `cap` bytes, so a truncation can start mid-line;
- the manager: stopping wins over everything, an active cooldown blocks, a
  cooldown that expires at the current instant no longer blocks, a dead
  transport is closed and replaced, a failed start closes the fresh transport
  and records `now + 5 s`, an unknown name has no cooldown;
- the warmup: names are unique across both config sources, the deadline is per
  started server (`spawn + 10 s`, computed after the spawn), `now >= deadline`
  aborts the discovery, a failed spawn contributes no tools, and the registry is
  reported only after all configs.

## Gate

```
bash tests/build.sh
```

`tests/fixture.cpp` carries a transcription of the native functions on
`std::string`, `std::map` and `std::filesystem`, with the source line cited at
every function. The resolver is checked against a real temporary tree
(`<tmp>/fg_ms617_fs`) created by the fixture; candidates are compared as a list
and then resolved against that tree on both sides. Everything else is compared
byte for byte in one process:

```
FG_MSTDIO cases=94 failures=0
```

Corpus: 18 resolver rows (exact, extension, path, give-up, order, PATHEXT order
and case, default PATHEXT, empty tokens, trailing separator, no dirs, two
candidate-list comparisons); 8 environment merges; 15 write scripts (zero size,
single and partial writes, zero return, EINTR, POLLOUT, timeout, HUP, ERR, NVAL,
poll failure, poll EINTR, running cleared, stalled script, clamped write);
28 pump scripts (two lines, full and split CRLF, empty lines, CR-only lines,
partial at EOF, chunk split, consumer limits, EOF, read error, POLLHUP, ERR,
NVAL, poll failure, EINTR, EAGAIN, timeout, poll EINTR, running cleared,
oversized drop and resync, bounded skip, cap boundaries, escapes); 5 stderr
tails; 11 manager rows; 8 warmup rows; the constants.

`tests/gate.log` is the recorded output of the last run (94 `OK` lines).

## Mutations

```
python3 tests/mutate.py 0 6     # 6/6 caught
python3 tests/mutate.py 6 12    # 6/6 caught
```

Twelve semantic mutations, two halves of six:

| # | mutation | caught by |
|---|---|---|
| 0 | an extension pass beats the exact name | `res_exact`, `res_order`, `res_pe_upper`, `res_dir_trailing_sep`, `res_cands_dirs` |
| 1 | a command with a separator also searches the path | `res_cands_path` |
| 2 | env overrides are emitted in input order | `env_sorted`, `env_empty_parent` |
| 3 | a parent key is never matched against the overrides | `env_override`, `env_sorted` |
| 4 | a write interrupted by EINTR aborts | `w_eintr` |
| 5 | a zero return from write is retried | `w_zero_return` |
| 6 | a pump line keeps its carriage return | `p_crlf_full`, `p_crlf_split`, `p_cr_only_line` |
| 7 | the resync drops the rest of the buffer | `p_oversize_resync` |
| 8 | the cooldown includes its deadline instant | `mgr_cooldown_expired` |
| 9 | a failed start records no cooldown | `mgr_failed_start`, `mgr_started_not_alive` |
| 10 | the warmup budget shrinks to five seconds | `warm_one_ok`, `warm_deadline_hit`, `warm_just_below`, `warm_stopping`, `warm_duplicate`, `warm_two` |
| 11 | a duplicate server name is warmed up twice | `warm_duplicate` |

`mutate.py` restores the source after every mutation and rebuilds once at the
end; a mutation that breaks the build counts as caught.

## Recorded limits

- The spawn, the process handle, the three threads and their joins stay native
  with `common_subproc`; this slice decides candidate order, framing and loop
  state only. The port never stats a file: it returns candidates and the caller
  resolves the first existing one.
- `resolve_command` models `SearchPathW` with a caller supplied directory list:
  the application directory and the Windows system directories are collapsed
  into the first supplied entry, and the native Windows branch also calls
  `subprocess_option_search_user_path`, which this model does not reproduce.
  Case-insensitive lookup is applied by the fixture (Windows semantics); the
  port keeps the raw PATHEXT spelling.
- The Windows `PIPE_NOWAIT` branch of `mcp_write_all` is not modelled, only the
  POSIX `O_NONBLOCK` + `poll` path. The 50 ms poll interval and the 10 ms
  Windows sleep are constants inside the decision, not clock inputs; the scripted
  result stands for the wait.
- The pump takes `max_line` as an argument (native 8 MiB) so the corpus can
  exercise the cap with a few bytes; the model buffer is 1024 bytes, so a chunk
  larger than that is truncated. `on_line` is modelled as a delivery count, the
  reader/writer `close_write` side effects are represented only by the pump
  state, and the native log lines (`SRV_DBG`, `SRV_WRN`) are not reproduced.
- `err_tail` takes `cap` as an argument (native 4096) and models the tail bytes
  only, not the logging.
- The manager models `start()` as `start_ok && is_alive` and the
  close-after-unlock ordering as `close_old`/`close_new` flags; the mutex, the
  `shared_ptr` lifetimes, the diagnostics text and `call_tool` stay in S3/S5.
- Warmup rows stand for configs: the two config sources (file, inline JSON) are
  represented by row order, and a partial tool list from an aborted discovery is
  reported as zero tools because the native return value there is not a decision
  of time.
- Not covered here at all: `server_mcp_stdio::start/close/join_pumps` side
  effects, `send_rpc` over a live pipe (S3 models the verdict), the HTTP
  handlers, `server_tools` runtimes and the decision init/embeddings.

## Files

| path | what |
|---|---|
| `source/mcp-stdio.goose` | the port |
| `tests/fixture.cpp` | the native transcription, the test tree and the corpus |
| `tests/build.sh` | builds the module and the fixture at `-O1`, runs the gate; mutants build at `-O0` |
| `tests/mutate.py` | the twelve mutations, `mutate.py [lo hi]` |
| `tests/gate.log` | the recorded gate output |
| `REPORT.md` | result, table, hashes |

`build/` is generated and not part of the experiment.
