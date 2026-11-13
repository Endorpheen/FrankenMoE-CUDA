# EXP470: response assembly and request logging in Goose (P8-02 twelfth slice)

The last owned decisions on the response path are how a body is framed, which
status it gets, which content type wins and which routine requests stay out of the
log.

## What was ported

`src/response.goose`, from `process_handler_response`, `log_server_request` and the
`server_http_res` helpers:

| decision | rule |
|---|---|
| body framing | a chunk generator means chunked, otherwise one body |
| `X-Accel-Buffering` | forced to `no` for a stream, overriding a user value |
| status | 200 on success, the payload code on an error |
| logging | health, metrics, props (GET/POST) and models GET are not logged |
| content type | the stream's declared type wins over the handler's value |

The socket, header serialization and body transfer stay native.

## Gate

`tests/response_fixture.cpp` transcribes the native expressions and compares.

| corpus | cases |
|---|---|
| framing and accel flags | 4 |
| status matrix (success / error x six codes) | 12 |
| log-skip over route categories 0..9 | 10 |
| content-type precedence | 4 |
| total | **28 cases, 30 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |

Mutations 6/6 caught: plain body sent chunked (1), accel inverted (2), error status
forced to 200 (6), models GET no longer skipped (1), unknown route skipped (5),
handler type beating the stream (2).

## Scope

What remains in P8-02 is the httplib transport itself (sockets, TLS, keep-alive,
chunked framing on the wire) and its thread-pool task queue. Both are the vendored
transport boundary; the decisions above it are now Goose.
