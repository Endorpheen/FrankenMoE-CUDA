# P8-07 preparatory slice — local tools: definitions and dispatch

Base commit `6c217e30`. Read-only analysis of the native server plus a
compilable draft of the **pure** parts of `tools_io_basic`: the tool table,
argument validation, result formation, and the text/format helpers. Every file
operation is a parameter of these functions, so nothing here touches the file
system, spawns a process or needs a model.

Native source: `upstream/llama.cpp/tools/server/server-tools.cpp` (2172 lines);
plumbing in `server-tools.h`, `server-common.h:41` (`json_value`), HTTP wrapper
at `server-tools.cpp:2080-2172`.

## 1. The seven tools

Registration order and flags (`server-tools.cpp:1957-1963`, class flags at
`869-1963`):

| # | name | display | write | cwd | stream |
|---|---|---|---|---|---|
| 0 | `read_file` | Read file | no | yes | no |
| 1 | `file_glob_search` | File search | no | yes | no |
| 2 | `grep_search` | Grep search | no | yes | no |
| 3 | `exec_shell_command` | Execute shell command | **yes** | yes | **yes** |
| 4 | `write_file` | Write file | **yes** | yes | no |
| 5 | `edit_file` | Edit file | **yes** | yes | no |
| 6 | `get_info` | Get Runtime Info | no | yes | no |

### 1.1 Parameter schemas and validation

| tool | parameters (type, default) | required | validation and clamps |
|---|---|---|---|
| `read_file` (`:874-893`) | `path` string; `start_line` int=1; `end_line` int=-1 (unlimited); `append_loc` bool=false | `path` | none beyond presence; `resp_type` is **not** a model argument — it comes from the `x-resp-type` header (`:901`, `:2110-2116`) |
| `file_glob_search` (`:984-1008`) | `path` string; `include` string="**"; `exclude` string=""; `type` string="file"; `max_depth` int=0; `limit` int=100 | `path` | `limit >= 1` or `invalid limit: <n> (expected 1 or more)` (`:1031-1034`); `type ∈ {file,dir,all}` or `invalid type: <t> (expected "file", "dir" or "all")` (`:1041-1046`); `max_depth = max(0, …)`; `limit = min(limit, 100)` |
| `grep_search` (`:1104-1128`) | `path`, `pattern` strings; `include`="**"; `exclude`=""; `return_line_numbers` bool=false; `literal` bool=false; `ignore_case` bool=false; `context_lines` int=0 | `path`, `pattern` | `context_lines = max(0, …)`; `literal` escapes the regex specials `\^$.|?*+()[]{}` (`:1157-1165`); an invalid regex is `invalid regex: <e.what()>` (`:1170-1172`) |
| `exec_shell_command` (`:1257-1271`) | `command` string; `timeout` int=10; `max_output_size` int=16384 | `command` | `timeout = min(timeout, 60)`; `max_output_size = min(…, 16384)` (`:1279-1281`); `runtime` is header-only |
| `write_file` (`:1331-1345`) | `path`, `content` strings | both | none |
| `edit_file` (`:1375-1402`) | `path` string; `edits` array of `{old_text,new_text}` | both | non-empty array; every `old_text` non-empty; matches must be unique and non-overlapping (`:1412-1489`) |
| `get_info` (`:1709-1719`) | none | none | none |

`json_value` semantics (server-common.h:41-52): a missing or `null` member is
the default; a **wrong-typed** member logs a warning and is also the default.
A missing **required** member is not a tool error: `params.at` throws, which the
HTTP wrapper turns into 400 (`:2143-2146`).

### 1.2 Error classes

| class | native | status | shape |
|---|---|---|---|
| unparsable body / wrong JSON | `common_json_error` | 400 | `format_error_response(…, ERROR_TYPE_INVALID_REQUEST)` (`:2143`) |
| missing required member | `params.at` throws `common_json_error` | 400 | same (`:2143`) |
| unknown tool | `find_tool` throws `std::invalid_argument` | 404 | `:2144-2146` |
| tool-level refusal | a normal return | 200 | `{"error":"…"}` (`:917`, `:933`, `:1031`, `:1044`, `:1189`, `:1352`, `:1414`, …) |
| unexpected exception | any other `std::exception` | 500 | `:2147-2150` |

Tool-level error texts implemented here and checked by the gate:
`cannot stat file: <p>`, `failed to open file: <p>`, `file too large (N bytes, max
16384). Use start_line/end_line to read a portion.`, `file too large (N bytes,
max 33554432)` (base64), `invalid limit: N (expected 1 or more)`, `invalid type:
<t> (expected "file", "dir" or "all")`, `"edits" must be a non-empty array`,
`edits[i].old_text must not be empty`, `could not find edits[i].old_text in <p>,
it must match the file's current content exactly`, `found N occurrences of
edits[i].old_text in <p>, it must be unique`, `edits[a] and edits[b] overlap in
<p>; merge them into one edit or target disjoint regions`, `failed to write
file: <p>`.

### 1.3 Path safety

`tools_io_basic::resolve` (`:301-330`):

1. `expand_home`: a leading `~` becomes `$HOME` (`:127-136`);
2. a relative path is joined to the IO's `cwd` (empty = the server's current
   directory), so `cwd` is the *base*, not a jail;
3. `lexically_normal()`, then a trailing `..` is collapsed one level.

There is **no sandbox**: an absolute path is returned unchanged and `..` may
climb above `cwd`; nothing compares the result with a root prefix. The only
mitigations are outside this function: `cwd`/`runtime`/`resp_type` are stripped
from model-supplied params and taken from headers (`:2088-2117`), and the
`permission_write` flag marks the three writing tools. That is a security
finding for the report, not something the port can fix silently.

### 1.4 Output limits

| limit | value | where |
|---|---|---|
| `read_file` plain | 16 KiB, then `[output truncated]` | `:866`, `:961-964` |
| `read_file` base64 | 32 MiB input ceiling | `:867`, `:910-914` |
| `file_glob_search` | 100 results, then a "limit reached" tail | `:973`, `:1073-1078` |
| `grep_search` | 100 matches, then a "limit reached" tail | `:1090`, `:1222-1226` |
| `exec_shell_command` | 16 KiB output (`[output truncated]`), 60 s | `:1242-1243`, `:278-280` |
| `get_info` | 4 KiB, 5 s | `:1697-1698` |

## 2. What the draft implements

`tools_pure.goose`, 1052 lines, `goose check` clean (191 specializations):

- `tool_name` / `tool_index` / `tool_flags` — the table above and dispatch;
  an unknown name is `-1`, the native 404.
- `tool_validate(idx, params, …)` — parses the params with the accepted JSON
  parser (`jsonparse.goose`, byte copy of `goose/json/json-parser.goose`,
  SHA-256 `ee56f4e4…16bfd70e`) and validates every tool: required members,
  `json_value` defaults and wrong-type fallbacks, the clamps, the enum, the
  `edits` structure. Status is `0` ok, `-1` unparsable/not an object (400),
  `-2` missing required member (400), `-3` tool error with the native message.
- `tool_edit_check` — uniqueness and overlap against the **original content**
  supplied by the caller, with the three native messages; returns the sorted
  match positions.
- `tool_read_text` — the 1-based inclusive line range, the `N→` prefix, the
  16 KiB ceiling and the truncation marker.
- `tool_read_guard` — stat failure, the 16 KiB refusal (only when `end_line` is
  unlimited) and the 32 MiB base64 ceiling.
- `tool_literal_regex` — the literal-mode escaping.
- `tool_exec_tail`, `tool_glob_tail` — the native tails.
- `tool_json_*` — the result shapes in native key order: error, plain text,
  write, edit, info, base64 size; `tool_json_base64` emits an empty payload
  because base64 encoding is a separate port.

Flat conventions: strings the validator keeps are copied into the caller's
buffer and recorded as `(offset, length)` pairs from `st[20]`; the response
message is in `msg` with its length in `st[19]`; per-tool numbers stay in
`st[10..19]`; the edit list occupies `st[30 + 4i]`.

## 3. Gate

`tests/tools_fixture.cpp`, 12 cases, 92 checks, all passing:

```
FG577_TOOLS cases=12 values=92 failures=0
```

| # | case | checks |
|---|---|---|
| 1 | dispatch and flags | indices, unknown → -1, names, write/cwd/stream bits |
| 2 | `read_file` defaults | path, start 1, end -1, `append_loc`, `resp_type` → base64 |
| 3 | missing required | `-2` for path/pattern/content/edits, `-1` for bad JSON |
| 4 | `read_file` text | ranges, `N→`, start past EOF, truncation marker and budget |
| 5 | `read_file` guards | stat failure, 16 KiB refusal, range exemption, 32 MiB ceiling |
| 6 | glob validation | limit 0 message, clamp 100, `max_depth` clamp, bad type message |
| 7 | grep validation | defaults, `context_lines` clamp, `literal` escaping |
| 8 | exec clamps | timeout 10 → 60, output 16384 cap, small values kept |
| 9 | write args | path/content, byte count, empty content |
| 10 | edit args | non-empty array, empty `old_text` message, edit count |
| 11 | edit matching | not found, 3 occurrences, overlap, disjoint positions |
| 12 | result JSON | error/plain/write/edit/info/base64/exec/glob shapes |

Two of the checks were wrong first and the gate caught them: the truncation
marker is 18 bytes (`[output truncated]`), and a file with no newline is one
line, so the 16 KiB refusal applies before any line formatting.

## 4. Ready, and not ready

Ready for integration (pure, gated, no substrate):

- the tool table, names and flags;
- all seven validators and their exact messages;
- the edit uniqueness/overlap checks (exact matching only);
- `read_file` line formatting and size guards;
- the result JSON emitters and the glob/exec tails;
- the literal regex escaping.

Needs the substrate (deliberately left as parameters):

- `tools_io`: `is_directory`, `is_regular_file`, `file_size`, `read_file`,
  `write_file`, `resolve`, `list_entries`, `run` — the draft takes content,
  size, entry lists and match lists as arguments;
- the directory walk: `list_entries` with the junk-directory set and
  `max_depth`, gitignore filtering;
- glob matching (`glob_match`, `path_glob_match`), the regex engine
  (`std::regex` ECMAScript), and the base64 encoder;
- the fuzzy edit path (`normalize_for_fuzzy_match`,
  `apply_replacements_preserving_unchanged_lines`) — the draft's checks use the
  raw bytes, so a fuzzy-only match is reported as "not found" here;
- `get_info`'s `uname -a`/`pwd` and the OS/cwd strings;
- the HTTP wrapper: header overrides, streaming, status mapping, `find_tool`.

## 5. Risks

1. **No sandbox.** `resolve` allows absolute paths and `..` above `cwd`; a port
   that adds a root check changes behaviour for every existing client. This is
   the largest security decision in P8-07 and it is not in the slice.
2. `json_value`'s wrong-type fallback silently substitutes the default; a port
   that validates types strictly would reject calls the server accepts today.
3. The edit matcher's fuzzy normalisation (trailing whitespace, smart quotes,
   dashes, special spaces) is not ported; "unique" and "overlap" are exact here,
   which is stricter and can reject real calls.
4. Limits are constants in five places; a port must keep the 16 KiB / 32 MiB /
   100 / 60 s table in one owner or the tails will drift from the guards.
5. The schema text (descriptions and property sets in `get_definition`) is not
   emitted; only the validation contract is. A model-facing schema mismatch
   would only show up after the emitter slice.

## 6. Artifacts and hashes

| file | SHA-256 |
|---|---|
| `tools_pure.goose` (1052 lines) | `d42b37fac1a3201b36eaf3b4158caf427860144c524502f61e101993e9f61903` |
| `tests/tools_fixture.cpp` (379 lines) | `c1394c940edf6c61d6ecc982d3e067962905c4872c7a4188be7607aa99919c6c` |
| `jsonparse.goose` (byte copy of the accepted parser) | `ee56f4e479dd05b65f57589ca4916767d923d13e73187087a95ab27416bfd70e` |

`goose check` is clean; the fixture was run through the usual light standalone
(two modules, one gcc call). Binaries and the generated C were deleted.
