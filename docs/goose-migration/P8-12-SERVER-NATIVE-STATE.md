# P8-12: the native server implementation, checked by name

Checked 2026-11-19 on `main`, text level, no build.

## The shipped server

`llama-server` is built from `vendor/flint/tools/server/`. Of those sources, exactly two reach
Goose decisions:

```
vendor/flint/tools/server/server-context.cpp   (speculative synth door, speculative doors)
vendor/flint/tools/server/server-models.cpp    (model manager doors)
```

`server-http.cpp` and `server-tools.cpp` have **zero** Goose call sites.

## The two named bodies

| item | native | Goose counterpart | state |
|---|---|---|---|
| `find_tool` | `server-tools.cpp:1935`, called at `:2115` | `goose/server/tools-pure.goose` - `tool_name` is called from `server-mcp.cpp` and `server-tools.cpp`, `tool_index` from `server-tools.cpp` and `common/chat-auto-parser-generator.cpp` | **the native scan is still called, and the Goose lookups are wired next to it**: two of the sixteen doors are reached, which the first version of this note missed because it searched only the door named `find_tool` |
| origin / CORS | `server-http.cpp:75` `origin_is_localhost`, used at `:334-336` | `goose/server/cors-policy.goose`; `cors_allow_origin` is called from `integration/goose-standalone/cli/serve-main.c` | native in `llama-server`; **one of five doors is wired, in the standalone CLI** - the first version said all were unwired |

## What that means for the requirement

The requirement asks for the server logic to run on Goose, with the replaced C++ absent from the
final execution path. Neither item is there yet: both native bodies are on the served path of the
shipped binary and neither file calls the composed decisions.

The removal order stays as the project rules put it (rule 15, rule 28): the switch comes first, then
a differential gate over the same inputs, then the native body goes. For these two the differential
is cheap to define, which is why they are named as the next server work rather than as a rewrite:

- `find_tool`: the same tool list and name, native verdict against `tools-pure.goose`'s index, for
  every registered tool plus an unknown name (the 404 case);
- origin: the same Origin header and parsed URL against `cors_allow_origin`, including the
  `"localhost"` and `"*"` settings, and the preflight method.

## What is not claimed

That any of this is measured, wired or removed. `server-http.cpp` and `server-tools.cpp` still hold
the decisions they held before, and the two files that do call Goose are named above so the
distinction is on the record.

## The method that produced these corrections

Three records (P8-06, P9-07, P8-09) and two more (here and in P9-03) shared one flaw: the call-site
search covered `vendor/flint/src` and `runtime/` but not headers and not `integration/`. Every
"nothing calls it" claim was re-run with `*.cpp`, `*.c`, `*.h`, `*.hpp` over
`vendor/flint`, `runtime` and `integration`, and the numbers above are that re-run.

The conclusions that survive: the shipped server path still does not call the web, metrics,
expert-cache or router-hook doors. The conclusions that did not survive: the counts. A count is a
fact about a search, and the search has to be stated with the number.
