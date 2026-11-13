# EXP469: streaming content-provider decisions in Goose (P8-02 eleventh slice)

A streamed response hands httplib a chunk generator and a write/complete pair.
The decisions inside that provider are small but visible under load: when to write,
when a failed write ends the stream without finishing the sink, and when the sink
is finished.

## What was ported

`source/provider.goose`, from `process_handler_response` and
`server_http_res::is_stream`:

| decision | rule |
|---|---|
| stream predicate | a response is a stream exactly when a chunk generator exists |
| nginx hint | a streamed response always sets `X-Accel-Buffering: no` |
| provider step | a non-empty chunk is written first; a failed write stops the provider without finishing the sink; no next chunk finishes the sink and stops; otherwise the provider asks for another call |
| completion | `on_complete` runs after the provider finishes for a stream, immediately for a plain body |

The sink, the socket and httplib's chunk framing stay native.

## Gate

`tests/provider_fixture.cpp` transcribes the lambda and compares both single
steps and whole provider sessions, including write failures and empty chunks.

| corpus | cases |
|---|---|
| stream predicate and header | 4 |
| single-step matrix (has_next x chunk x write_ok) | 8 |
| 400 random provider sessions (1..12 calls, empty chunks, failed writes, early finish) | 400 |
| completion ordering | 2 |
| total | **412 cases, 414 values, 0 failures** |
| ASan + UBSan (runtime, module, fixture) | 0 failures, rc 0 |

Mutations 7/7 caught: failed write continues the stream (96 failures), sink never
finishes (309), finish still asks for more (265), stream predicate inverted (2),
nginx header inverted (2), last chunk skipped (178), completion ordering swapped
(2).

## Scope

P8-02 still needs the httplib transport itself and the task queue. The provider's
write/complete calls, the socket and the chunk framing remain native.
