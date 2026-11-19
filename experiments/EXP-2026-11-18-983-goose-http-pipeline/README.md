# EXP-2026-11-18-983: buffered requests, so pipelining works

Status: gate PASS, 84 checks. The read loop keeps the remainder, which is what
pipelining needs.

## Hypothesis fixed before changes

EXP982 served several requests on one connection but only sequentially, and said so:
the handler read up to 8 KiB and parsed the first request line, so a second request
arriving in the same write was consumed and lost. If the read loop keeps the
unprocessed remainder and finds the request boundary itself, two requests in one
write must both be answered, in order.

## What was done

The single handler split into three:

- `hl_fill(cfd, req, have)` reads more bytes into `req[have..]` through the host
  scratch - Goose cannot take the address of its own slice - and returns the new
  length, or -1 on error or EOF;
- `hl_req_end(buf, len)` finds `CRLF CRLF` and returns the offset past it, or -1
  while the terminator has not arrived. No body is read, so the header block is the
  whole request;
- `hl_handle_req(cfd, req, ...)` is the decision body on a slice; it no longer reads.

`hl_serve_keep` now holds `have` across iterations: it reads until the terminator is
present, handles `req[0..end]`, then shifts the remainder to the front and continues.
`hl_serve_full` reads once and handles the first request.

## Result

| check | result |
| --- | --- |
| two requests in ONE write | **both served**, `200` then `404`, in order |
| two sequential requests | still served, in order |
| `max_requests = 1` | stops after one |
| gate | **84 checks, 0 failures** |
| canonical unit | 142 modules, 4303 specializations |

## Not claimed

The remainder buffer is the caller's `req` slice, so it is bounded by its length: a
client that pipelines more than that in one write overflows the buffer and the loop
stops rather than corrupting it. Requests with a body are still unsupported, so a
request whose body arrives with the headers is answered for the header block only.

Still no TLS, no `Connection: close` handling, and the shipping server still uses
httplib.

No model was started.

## Reproduction

    python3 tools/compose-goose.py OUT/unit.goose \
        goose/server/{http-request,http-response,http-listen,route-table,api-key-policy,cors-policy}.goose
    <goose> check OUT/unit.goose
    # build the gate as in EXP978 and run it
