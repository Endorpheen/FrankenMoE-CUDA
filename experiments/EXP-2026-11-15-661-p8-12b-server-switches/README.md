# EXP-2026-11-15-661 — server switches default to Goose and one native body removed

Base: `7f1ce109`. Isolated flint CPU tree; the repository was not modified. No
model, server or GPU run happened.

## What changed

Two call sites became runtime switches with Goose as the default (rule 10: the
flag exists only to disable):

- model status: `server-models.h` calls `fgs_model_status_text`; the native
  `server_model_status_to_string` body (the whole 7-status switch) is **removed**;
- Origin host: `server-http.cpp` calls the Goose `origin_is_localhost`; the
  native three-comparison body stays as the OFF arm behind
  `FG_GOOSE_SERVER_CORS_NATIVE=1`.

CMake defaults: `FG_GOOSE_SERVER_STATUS=ON`, `FG_GOOSE_SERVER_CORS=ON`;
`TOOLS`, `MIME` and `CACHE` remain OFF.

A defect from the previous step was found and fixed: `goose_init` was never
called, so the unit panicked on the first call. `fgs_init_early(argc, argv)` runs
at the start of `llama_server()`, plus a lazy start in the bridge.

## Result

| check | result |
|---|---|
| build + link, OFF / ON, phase A | 19 608 544 / 19 692 888 B, rc=0 |
| build + link, OFF / ON, phase B | 19 608 376 / 19 692 728 B, rc=0 |
| default configuration (no flags) | 19 688 608 B, `goose_init` = 1 |
| step-1 baselines reproduced | byte-for-byte (19 607 808 / 19 687 864 B) |
| gate, ON | 15/15 cases, 0 divergences, PASS (9 statuses + 3 localhost hosts + 3 refusals) |
| gate, OFF | 15 cases, 7 divergences, FAIL by design: the native status body is gone and the OFF arm answers `unknown` |
| mutations | 12/12 killed (halves 6/6 status, 6/6 CORS), control PASS |

The disassembly shows the ON path calling the Goose function, and the OFF path
loading the `unknown` literal in place of the removed body.

## Seam

The server unit is composed from the canonical server modules; the carrier
patches `patches/ports-context.diff` and `patches/ports-queue.diff` are the
merged server ports, and `patches/callsites.diff` is the switch diff. The
merged copies come from the flint revision, so the staged tree must be the flint
CPU copy, not the M1 tree (recorded in the report).

## Remaining

- Switch MIME, then Cache-Control, then the tool dispatch (last, because
  `find_tool` also serves MCP).
- Errors are reported through `fail()`/`abort()` where the native path returned a
  status; the host wiring must keep the calling convention.

## Files

`source/` — the `fg-goose-server` unit, switches and host; `patches/`;
`tests/` — gate, mutation harness and build; `evidence/` — build logs and call
audits.
