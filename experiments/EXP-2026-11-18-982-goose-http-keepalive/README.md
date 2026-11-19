# EXP-2026-11-18-982: several requests on one connection

Status: gate PASS, 78 checks. The listener is no longer single-shot.

## Hypothesis fixed before changes

EXP978-981 all served exactly one request and closed. A transport that hangs up after
every answer is not what clients expect, and it hides a real question: does the loop
keep the connection's state - the accepted fd - across requests without leaking or
closing it early.

## What was done

The one-request body became a private `hl_handle(cfd, ...)` that reads, parses,
decides and answers but **never closes** the connection; the fd belongs to the
caller. Two exports use it:

- `hl_serve_full` accepts one connection, handles one request, closes;
- `hl_serve_keep(fd, req, resp, keys, koff, origins, credentials, max_requests)`
  accepts one connection and handles up to `max_requests` on it, stopping when a read
  returns 0 (the peer closed) or the bound is reached.

A read returning 0 is now "the client closed", not "send a 400 to nobody": the
handler returns -1 and the loop ends.

## Result

| check | result |
| --- | --- |
| two requests on one connection | **both served**, in order: `200` then `404` |
| `max_requests = 1` | the loop stops after one |
| gate | **78 checks, 0 failures** |
| canonical unit | 142 modules, 4301 specializations, typecheck clean |

The client sends each request only after reading the previous answer, and it reads
exactly one response (headers plus `Content-Length` bytes), so the test exercises
sequential keep-alive rather than pipelining.

## What this does not cover, and why it matters

**Pipelining is not supported.** The handler reads up to 8 KiB and parses the first
request line; if a client sends two requests in one write, the second is consumed by
the same read and lost. Native httplib buffers the remainder. This is a real
limitation, stated rather than hidden, and the gate deliberately drives sequential
requests so it cannot be mistaken for pipelining support.

Also unchanged: no TLS, no body reading, no `Connection: close` handling - the server
keeps the connection until the client closes or the bound is reached.

## Reproduction

    python3 tools/compose-goose.py OUT/unit.goose \
        goose/server/{http-request,http-response,http-listen,route-table,api-key-policy,cors-policy}.goose
    <goose> check OUT/unit.goose
    # build the gate as in EXP978 and run it
