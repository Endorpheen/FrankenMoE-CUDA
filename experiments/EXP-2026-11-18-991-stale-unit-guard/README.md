# EXP-2026-11-18-991: A fresh timestamp over an old body, and a server that did not exist

Status: the canonical unit was stale while every date on it was newer than the
sources; the staleness guard now compares content; the listener finally has an
executable entry point. 38 254 checks, 0 failures.

## Three findings, in the order they appeared

### 1. The unit was stale, and its dates hid it

Looking for the listener's signatures in `build/goose-main/main.h`, only three of the
six exports were there - `hl_serve_keyed`, `hl_serve_full` and `hl_serve_keep` were
absent. The module exports all six. A fresh compose of the same module list contains
them:

    build/goose-main/main.c   0 occurrences of hl_serve_full, 0 of rq_host_violation
    fresh compose             5 and 6
    symbols                   1158 stale vs 1161 fresh

The guard in `tools/build-standalone-runtime.sh` compared **mtimes**. A unit that is
restored, copied or produced elsewhere has a fresh mtime and an old body, so neither
test could see it. The gates never noticed because `verify-unit-gates.sh` composes its
own unit straight from `goose/` - the tests were right and the artifact was wrong.

`tools/build-main-runtime.sh` now writes `.source-sha256`: one hash over the composer
and every `goose/**/*.goose` in sorted order. The standalone build recomputes it and
rebuilds on any difference.

    unit as found:  no .source-sha256
    DECISION:       rebuild
    after rebuild:  UP TO DATE

### 2. There was no executable entry point for the listener

The listener was a set of symbols in the artifact. Nothing opened a port. `nm` on
`frankenmoe-v4` showed `hl_serve_full` and `hl_serve_keep` defined and no binary ever
called them - a capability, not a path.

`integration/goose-standalone/cli/serve-main.c` is that path. It owns argv, the accept
loop and nothing else: parsing, routing, the key policy, CORS and the response format
are all Goose. Measured on a live socket:

    GET /health   ->  HTTP/1.1 200 OK / Content-Length: 2 / body "ok"
    --key secret, GET /props without it  ->  401
    --origin '*'  ->  Access-Control-Allow-Origin: *

It refuses port 9101 by name, because that port belongs to the runtime server (rule 21).

### 3. A defect I introduced in EXP987, found by running the new binary

The ordinary 200 carried `Access-Control-Allow-Credentials: false` and
`Content-Type: text/html`. Reading the native source:

    server-http.cpp:292-298   inside if (req.method == "OPTIONS"):
        Access-Control-Allow-Credentials, -Allow-Methods, -Allow-Headers,
        set_content("", "text/html")

Those three headers and the blank body belong to the **preflight** only. The
allow-origin header belongs to every response. EXP987 removed the `status == 204`
condition from the whole block, which was right for allow-origin and wrong for the
rest. The gate now asserts the absence of both on an ordinary 200.

This only surfaced because the server was run. The gate checked that the header was
present and would never have asked whether another one was.

## What the corrections taught about my own process

Linking the new `serve-main.o` against `main-gc.o` reproduced finding 1 immediately: the
server still emitted the preflight headers because `main-gc.o` was the object built
before the fix. Same failure, same cause, caught by the same kind of check - a live
response rather than a symbol table.

## Result

| gate | checks | result |
| --- | --- | --- |
| http-listen (+2 negative checks) | 149 | PASS |
| whole suite | **38 254** | **0 failures** (9 gates) |

Unit: 1161 globals, 0 duplicates, 146 extern. `frankenmoe-v4` 5 621 296 B,
`frankenmoe-serve` 1 271 880 B.

## Not claimed

The server reads no request bodies, so POST endpoints answer as if the body were
empty. There is no TLS and no worker pool: one connection at a time. `serve-main.c` is
a host adapter - it is debt, counted as such, and it holds no decisions.

`libstdc++` and `libgomp` are in `ldd`; both arrive with `libggml-base` and are
substrate, already measured in `docs/goose-migration/STANDALONE-DEPS.md`. The
script's old header comment claimed otherwise and now points at that document.

No model was started and no benchmark was run.
