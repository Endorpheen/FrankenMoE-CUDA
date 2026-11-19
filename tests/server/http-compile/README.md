# http-compile

**Requirement:** the guard for P8-09 and P8-12 - the shipped server sources must keep compiling.
**Produced by:** the P8-09 wiring of 2026-11-19, which had no check covering it.
**Tier:** fast - two translation units, syntax only, about two seconds.

## Why it exists

`server-http.cpp` and `server-tools.cpp` depend on a **generated** `ui.h`, so they cannot be compiled
outside a configured tree. Neither CI job covers them: the C++ job compiles `models/*.cpp` and the
Goose job compiles the unit. A change to `server-http.cpp` - the API-key door wiring, for instance -
was therefore unverified until this check existed.

## What it does

Syntax-checks both sources against a configured tree's `tools/ui` include path. The tree defaults to
`builds/flint-acc`; pass another as the first argument. Exit 77 when that tree has no generated
`ui.h`, because then the source genuinely cannot be compiled here.

## Run

```
bash tests/server/http-compile/run.sh                  # builds/flint-acc
bash tests/server/http-compile/run.sh <configured-tree>
```

Expected: `ok: server-http.cpp compiles` and `ok: server-tools.cpp compiles`, exit 0.
