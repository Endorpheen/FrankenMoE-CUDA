# EXP446: server pre-routing policy in Goose (P8-02 first slice)

Every HTTP request passes the pre-routing handler before any endpoint runs. It
sets CORS headers, answers `OPTIONS`, rejects requests while the model loads and
validates API keys. This experiment moves those decisions into Goose.

## What was ported

`source/prerouting.goose`, transcribed from `tools/server/server-http.cpp`
(`set_pre_routing_handler`, `middleware_server_state`,
`middleware_validate_api_key`, `origin_is_localhost`):

1. **CORS allow-origin mode**: `credentials && origins == "*"` echoes the request
   `Origin`; `origins == "localhost"` echoes only a localhost origin
   (`localhost`, `127.0.0.1`, `::1`, any port); otherwise the configured literal
   is emitted. The mode and length are returned so the wrapper sets the header.
2. **Origin host extraction** in Goose: optional scheme, optional userinfo,
   bracketed IPv6, `host:port`, authority terminated by `/`, `?` or `#`.
3. **OPTIONS short-circuit**: allow-credentials/methods/headers are emitted and
   routing stops with 200 before the gates, because browsers send no
   `Authorization` on preflight.
4. **Readiness gate**: 503 "Loading model", except frontend assets.
5. **API-key gate**: public and frontend paths skip it; otherwise the
   `Authorization` value (or `X-Api-Key` when Authorization is empty) is compared
   to a configured key after an optional `Bearer ` prefix is stripped.

Output words: `action` (continue/handled), `status`, allow-origin mode, literal
length, options-headers flag, reject reason.

## Gate

`tests/prerouting_fixture.c` transcribes the native handler and compares all six
words for every case.

| corpus | cases |
|---|---|
| 17 Origin strings (empty, localhost/loopback with ports, bracketed IPv6, `user:pass@`, `file://`, lookalikes `localhostevil.com`, `sub.localhost`, `null`, `127.0.0.2`) x 8 Authorization values x 3 keys x credentials x 3 CORS modes x 3 literal lengths x options x ready x frontend x public x 3 key counts | 352 512 |
| `X-Api-Key` fallback corpus (empty Authorization) | 153 |
| total | **352 665 cases, 2 115 990 values, 0 failures** |
| ASan + UBSan host build | 0 failures, rc 0 |

Mutations 7/7 caught: CORS star+credentials stops echoing (116 352 failures),
localhost mode accepts any origin (96 768), scheme bytes kept (110 592), OPTIONS
short-circuit removed (528 768), frontend exception lost (119 952), public paths
re-checked for keys (110 160), `Bearer ` prefix shortened to six bytes (11 016).

## Scope

This slice owns the pre-routing decision policy. Still native and therefore
P8-02 stays IN_PROGRESS:

- the HTTP transport itself (`httplib` server/socket/TLS, keep-alive, chunked
  bodies) — vendored external dependency, not owned code;
- the route table and handler registration, path parameters, streaming
  (`server-stream.cpp`) and the task queue;
- request body parsing per content type, multipart and the API schema;
- GCP AIP route override and the `/props` middleware payload.

## Reproduce

    bash experiments/EXP-2026-11-13-446-p8-02-prerouting/build_fixture.sh
    python3 experiments/EXP-2026-11-13-446-p8-02-prerouting/mutate.py
