# REPORT — P8-07 S5 part: stdio MCP transport shells

Base commit: `b68cffed`. Working copy: `EXP-2026-11-14-6xx-p8-07-mcp-stdio`
(scratch `/tmp/opencode/sub-617`), not committed.

## Result

The offline decision half of the stdio MCP transport and of the MCP manager is
ported to Goose and checked against a native transcription of
`tools/server/server-mcp.cpp`:

- gate: 94 cases, 0 failures (`FG_MSTDIO cases=94 failures=0`);
- mutations: 12/12 caught, two halves of six (`0 6` and `6 12`);
- no process, thread, socket or real pipe is involved. The resolver is checked
  against a real temporary directory tree, which is the only filesystem use.

## What was compared

| group | cases | native anchor |
|---|---|---|
| resolve candidates and resolution | 18 | `mcp_resolve_command` 379 |
| environment merge | 8 | `mcp_build_env` 446 |
| framed write loop | 15 | `mcp_write_all` 537 |
| NDJSON pump states | 28 | `mcp_pump_ndjson` 24 |
| stderr tail | 5 | `errlog_loop` 618 |
| manager create/cooldown | 11 | `get_or_create` 771 |
| warmup plan | 8 | `server_mcp::start` 693 |
| constants | 1 | 657-658, 31-32, 619, 676 |

The native side is a transcription on `std::string`, `std::map` and
`std::filesystem` with the source line at every function. Resolver candidates
are compared as an ordered list twice and resolved against the test tree on both
sides; every other group is compared byte for byte.

## Mutations

| # | mutation | caught by |
|---|---|---|
| 0 | extension pass before the exact name | `res_exact`, `res_order`, `res_pe_upper`, `res_dir_trailing_sep`, `res_cands_dirs` |
| 1 | path command falls back to the search path | `res_cands_path` |
| 2 | overrides unsorted | `env_sorted`, `env_empty_parent` |
| 3 | override never removes a parent entry | `env_override`, `env_sorted` |
| 4 | EINTR aborts the write | `w_eintr` |
| 5 | zero write return is retried | `w_zero_return` |
| 6 | CR kept on a pump line | `p_crlf_full`, `p_crlf_split`, `p_cr_only_line` |
| 7 | resync drops the rest of the buffer | `p_oversize_resync` |
| 8 | cooldown includes its deadline instant | `mgr_cooldown_expired` |
| 9 | failed start records no cooldown | `mgr_failed_start`, `mgr_started_not_alive` |
| 10 | warmup budget 5 s instead of 10 s | `warm_one_ok`, `warm_deadline_hit`, `warm_just_below`, `warm_stopping`, `warm_duplicate`, `warm_two` |
| 11 | duplicate config warmed up twice | `warm_duplicate` |

## What stays out

Spawn and `common_subproc`, the three real pump threads and their joins, the
live pipe `send_rpc`, the HTTP handlers, the tool runtimes and the decision
init/embeddings are the integration part of S5. They need real processes and are
not claimed here. The full list of model limits is in `README.md`
("Recorded limits"), including the caller-supplied search directories, the
argument-carried line cap and stderr cap, and the scripted poll/write results.

## Hashes (SHA-256)

| file | SHA-256 |
|---|---|
| `source/mcp-stdio.goose` | `99b9bcc3e555363c777b4da2334ec75783d2baeb998caeb67ecd108e80f34c92` |
| `tests/fixture.cpp` | `cedeace060b521ed863b7157e750ffcd5499ac0de413ca3689ccadb60579f84d` |
| `tests/build.sh` | `b24829e119ea58ec44ed1da738adb2d32ac1765320a4975f7ccba8cd20e48247` |
| `tests/mutate.py` | `1dbd072f5f7eff602c5974b6ce0b572bc2b27dfe1aa56acd136a71c6cd9f76f9` |
| `tests/gate.log` | `164c092ee7823bc1b5792aeac589b02a95ec5d42f6cd6b0130e8b8dd1888a1e2` |

Toolchain: goose 2026.3-string, gcc-13 -O1 for the module and g++-13 -O1 for the
fixture; mutants build at -O0, nlohmann/json from `llama.cpp-flint-cuda/vendor`.
