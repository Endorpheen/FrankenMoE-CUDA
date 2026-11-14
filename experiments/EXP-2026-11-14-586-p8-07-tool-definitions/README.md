# EXP-2026-11-14-586 — tool definitions and dispatch (P8-07 S2)

Native counterpart: `tools/server/server-tools.cpp` (the pure half: tool table,
argument validation, result formation and text helpers). Base commit:
`b3548dd1`.

## What this slice covers

`source/tools-pure.goose` (1052 lines, 16 exports) ports every decision of the
seven local tools without touching the filesystem:

| export | what it decides |
|---|---|
| `tool_name` / `tool_index` | the registration order and the reverse lookup for `read_file`, `file_glob_search`, `grep_search`, `exec_shell_command`, `write_file`, `edit_file`, `get_info` |
| `tool_flags` | write / cwd / stream bits (exec = 7, read-class = 2, write-class = 3) |
| `tool_validate` | the per-tool argument validation: required members, types, defaults and clamps (glob depth/limit, grep context, exec timeout 60 and 16 KiB output, read line range) |
| `tool_edit_check` | old-text occurrence count and the exact `could not find edits[i].old_text` message |
| `tool_read_text` / `tool_read_guard` | line-range assembly, the 0-based end-line rule, base64 selection and the stat/size guards with their messages |
| `tool_literal_regex` / `tool_exec_tail` / `tool_glob_tail` | the text helpers |
| `tool_json_error` / `tool_json_plain` / `tool_json_write` / `tool_json_edit` / `tool_json_info` / `tool_json_base64` | the exact result JSON member order and escaping |

`source/jsonparse.goose` is the byte-identical accepted parser (EXP493), kept
next to the module because Goose resolves `import jsonparse` by module name.

## Gate

`tests/build.sh` builds the pair with `--standalone`, compiles at `-O0` and runs
`tests/tools_fixture.cpp`, which covers dispatch, defaults, missing members,
validation, edit matching and the result JSON — 12 cases, 92 compared values:

```
FG577_TOOLS cases=12 values=92 failures=0
```

No filesystem, no process and no model: file operations are parameters of the
ported functions.

## Mutations

`tests/mutate.py 0 6|6 12` holds 12 semantic mutations in two halves. Each one
is checked with `goose --standalone`, rebuilt at `-O0` in its own work directory
and required to fail the gate.

```
python3 tests/mutate.py 0 6    # 6/6 caught
python3 tests/mutate.py 6 12   # 6/6 caught
```

## Recorded limits

- Only the pure half is ported; `tools_io_basic` file operations, the
  isolate/container/ssh backends and the runtimes stay native and belong to the
  same requirement's S5.
- `tool_validate` needs the JSON parser; tokenizing preserved strings and
  grammar triggers stays native.
- Expectations come from the native source with cited lines; this is not a
  byte-differential against a linked server.
