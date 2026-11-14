# EXP-2026-11-14-572 — CLI line protocol (P9-06 S2)

Native counterpart: `runtime/cli/main.cpp` (stdin request reader at 124-215 and
268-283, stdout emitters at 79-118 and 240-355), plus
`upstream/BigMoeOnEdge/docs/telemetry.md` §"Session mode". Base commit:
`ea075cb6`.

## What this slice covers

`source/cli_protocol.goose` (617 lines) parses one stdin line and emits the six
session messages:

| export | native | what it does |
|---|---|---|
| `cli_read_line` | `main.cpp:124-215, 268-283` | one line into a flat command: kind (generate/cancel/close/skip), id, n_predict, think, clear_kv, prompt bytes |
| `cli_emit_ready` | `main.cpp:249-255` | `BMOE_READY` with load_s, arch, n_ctx, think_ctl, n_expert_used |
| `cli_emit_begin` | `main.cpp:310` | `BMOE_BEGIN {"id":N}` |
| `cli_emit_load` | `main.cpp:103` | `BMOE_LOAD`, only when the token read something |
| `cli_emit_progress` | `main.cpp:107-118` | the 12-field per-token line, the optional `"reset":1`, two escaped deltas |
| `cli_emit_done` | `main.cpp:337-350` | the summary with cancelled, tokens, the timing split and the final reasoning/text |
| `cli_emit_error` | `main.cpp:327` | `BMOE_ERROR` with the recoverable/fatal flag |
| `cli_error_recoverable` | `main.cpp:323-327` | the two substrings the session survives |
| `cli_progress_delta` | `main.cpp:79-118` | suffix when both fields extend the snapshot, otherwise the full text with `"reset":1` |

The JSON text is parsed with the accepted parser, not a new one: `jsonparse` is
a byte-identical copy of `goose/json/json-parser.goose` (EXP493), imported as a
module. The canonical layout keeps the copy next to the protocol in `goose/cli/`
because Goose resolves `import jsonparse` only by module name in the same
directory or in stdlib.

Native behaviour copied on purpose: the delta decision is `is_extension` per
field (a shrink in either one forces the full snapshot); `BMOE_LOAD` is emitted
when bytes were read or IO time was measured; a refused request is non-fatal
only for "empty prompt" and "exceeds the session n_ctx"; `clear_kv` defaults to
true, `think`/`n_predict` to the process flags, `id` to 0; a line without a
usable `cmd` is skipped.

## Gate

`tests/build.sh` builds the two-module pair with `goose --standalone` (Goose
pulls `jsonparse.goose` from the same directory), compiles at `-O1`, and links
`tests/protocol_fixture.cpp`. The expected stdout is built with `snprintf` from
the same format strings the carrier prints, and the fixture's `json_escape`
escapes the oracle side, so the comparison is libc printf against the Goose
emitter rather than the emitter against itself.

```
FG577_S2 cases=17 values=26 failures=0
```

## Mutations

`tests/mutate_s2.py 1|2` holds 7 + 8 mutations; each rewrites the protocol,
rebuilds the pair and reruns the gate.

```
python3 tests/mutate_s2.py 1   # 7/7 caught
python3 tests/mutate_s2.py 2   # 8/8 caught
```

## Recorded limits

- The CLI flag surface (63 long flags), engine work and the model fields are
  other P9 requirements; this slice owns only the session line protocol.
- The gate runs at `-O1`; the 17 cases cover the protocol, not tokenization.
- `source/jsonparse.goose` is a copy of the canonical parser; the tree check
  verifies it against EXP493.
