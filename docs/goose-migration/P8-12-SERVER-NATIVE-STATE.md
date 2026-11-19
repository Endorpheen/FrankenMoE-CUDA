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
| `find_tool` | `server-tools.cpp:1935`, called at `:2115` | `goose/server/tools-pure.goose:92` - dispatch by name, `-1` meaning "the native `find_tool` would throw" (HTTP 404) | **native, still called**; the port exists and describes the native behaviour, which makes it a ready differential target |
| origin / CORS | `server-http.cpp:75` `origin_is_localhost`, used at `:334-336` | `goose/server/cors-policy.goose` (`cors_is_localhost`, `cors_allow_origin`, `cors_is_preflight`) | **native, still called**; the Goose decisions are composed and unwired |

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
