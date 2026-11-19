# EXP-2026-11-18-987: CORS on every response, and Connection: close

Status: gate PASS, 107 checks. Two httplib behaviours the listener did not have.

## Hypothesis fixed before changes

EXP981/985 emitted CORS headers only on the preflight, and nothing honoured
`Connection: close`. Native httplib sets the CORS headers on every response and closes
when the client asks. If the listener does the same, an ordinary `GET` with a policy
must carry the header, and a request saying `Connection: close` must be answered with
that header and must stop the keep-alive loop.

## What was done

- the CORS block lost its `status == 204` condition: `cors_allow_origin` is asked for
  every response, and the headers are written when it claims them;
- the header scan records `Connection: close` (case-insensitive name and value), the
  response carries `Connection: close`, and the handler returns `-(len + 1)` to mean
  "written, and the connection must close". Both callers decode that: `hl_serve_full`
  returns the length, `hl_serve_keep` counts the request and breaks.

The `-(len + 1)` encoding keeps a zero-length answer distinguishable from an error,
which matters because the handler already uses -1 for failure.

## Result

| check | result |
| --- | --- |
| `GET /health` with policy `*` | 200 **with** `Access-Control-Allow-Origin: *` |
| `Connection: close` + a pipelined second request | **only the first served**, response says `Connection: close`, no 404 |
| gate | **107 checks, 0 failures** |
| whole suite | **38 178 checks, 0 failures** (8 gates) |

## A replacement that silently did nothing

The first attempt added the `Connection` branch with a plain string replace in a
script without an assertion, and the pattern did not match - so the scan was never
extended while the emission code was. The gate caught it: `served == 2` and no
`Connection: close` in the response.

This is the same failure the series keeps recording: the code compiled, the typecheck
was clean, and the feature was half-present. The difference here is that the tool that
should have caught the edit - an assertion - was the one I omitted, not a check.

## Not claimed

`Connection: keep-alive` in a request is not treated specially; the connection stays
open, which is what keep-alive means anyway. There is still no TLS and no body
reading, and the shipping server still uses httplib.

No model was started.

## Reproduction

    bash tools/verify-unit-gates.sh
    sed -n '/^gate/,/^result/p' <log>
