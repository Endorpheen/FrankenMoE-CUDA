# EXP-2026-11-18-981: CORS headers on the preflight, decided in Goose

Status: gate PASS, 69 checks. `cors_allow_origin`, `cors_credentials_text` and
`cors_preflight_type` now decide what the listener writes.

## Hypothesis fixed before changes

EXP980 answered a preflight with 204 but no headers, which is useless to a browser.
The three CORS decisions existed, gated, in the unit. If the listener calls them,
then a literal policy must be emitted as-is, a wildcard with credentials must echo
the request's Origin, and an absent policy must emit nothing.

## What was done

- `hl_serve_keyed` delegates to a new
  `hl_serve_full(fd, req, resp, keys, koff, origins, credentials)`; an empty
  `origins` means "no CORS", which is the no-policy server;
- the header scan now records `Origin` beside `Authorization` and `X-Api-Key`;
- on a 204 the listener asks `cors_allow_origin` for the header value and emits
  `Access-Control-Allow-Origin`, `Access-Control-Allow-Credentials` (from
  `cors_credentials_text`) and `Content-Type` (from `cors_preflight_type`), but only
  when the decision claims the header.

## Result

| request | policy | answer |
| --- | --- | --- |
| `OPTIONS /health` | `*`, no credentials | 204, `Allow-Origin: *`, `Allow-Credentials: false` |
| `OPTIONS /health` + `Origin: http://example.test` | `*`, credentials | 204, the origin is echoed, `Allow-Credentials: true` |
| `OPTIONS /health` | none | 204, no CORS header at all |
| gate | | **69 checks, 0 failures** |

Canonical unit: 142 modules, 4290 specializations, typecheck clean.

## A bug of mine, found by the gate

The first version emitted an empty allow-origin instead of the echoed Origin. Cause:
the header scan sat inside `if status == 200`, and a preflight is 204, so `origin`
was never captured. The scan now runs for every parsed request, before routing.

That is worth recording because the failure was silent in exactly the way this series
keeps meeting: the header was present, well-formed, and empty.

## Not claimed

Origin localhost reflection (`cors_is_localhost` over the parsed Origin host) is not
wired: `origin_local` is passed as 0, so the `cors_origins == "localhost"` mode still
refuses rather than reflecting. CORS headers are emitted only on the preflight, not
on ordinary responses. Still one request per connection, no TLS, no body.

The shipping server still uses httplib.

No model was started.

## Reproduction

    python3 tools/compose-goose.py OUT/unit.goose \
        goose/server/{http-request,http-response,http-listen,route-table,api-key-policy,cors-policy}.goose
    <goose> check OUT/unit.goose
    # build the gate as in EXP978 and run it
