# EXP-2026-11-18-980: the listener authorises, and answers a preflight

Status: gate PASS, 54 checks. Two more existing Goose decisions now decide on the
request path: the API-key middleware and the CORS preflight.

## Hypothesis fixed before changes

EXP979 made the listener route, but every route was open: `key_authorize` and
`cors_is_preflight` existed, gated and in the unit, and nothing on the request path
called them. If the listener calls both, then a configured key set must produce 401
without a credential, 200 with the right one, and an OPTIONS preflight must be
answered before routing.

## What was done

- `hl_serve` keeps its signature and delegates to new
  `hl_serve_keyed(fd, req, resp, keys, koff)`, with an empty key set - the format
  `key_scan` reads: `koff[i]..koff[i+1]` is key `i`, and `koff.len <= 1` means
  "no key configured", which native skips validation for;
- after routing succeeds, the listener scans the header block with
  `rq_parse_header`, picks `Authorization` and `X-Api-Key` (names compared
  case-insensitively, RFC 9110 5.1), and calls `key_authorize`; its 0 becomes 401;
- `OPTIONS` is checked before the method table with `cors_is_preflight`, and a
  preflight is `204`, which is what the native server returns.

## Result

| request | key set | answer |
| --- | --- | --- |
| `GET /props`, no credential | `secret` | `401 Unauthorized` |
| `GET /props`, `Bearer secret` | `secret` | `200 OK` |
| `GET /props`, `Bearer wrong` | `secret` | `401 Unauthorized` |
| `GET /health`, no credential | `secret` | `200 OK` - the path is public |
| `GET /health`, no key set | none | `200 OK` |
| `OPTIONS /health` | none | `204 No Content` |
| gate | | **54 checks, 0 failures** |

Canonical unit: 142 modules, 4287 specializations, typecheck clean.

## One test of mine was wrong, and the module was right

The first version asserted that `GET /health` with a key configured returns 401.
It returns 200, because `key_public` treats `/health` and `/v1/health` as always
public - a native rule I had not read before writing the assertion. The 401 cases
now use `/props`, and an explicit case records the public-path rule so the
behaviour is pinned rather than assumed.

## Not claimed

The public-path list passed to `key_authorize` is empty: only the two built-in
public paths apply, not the served UI assets. CORS headers are not emitted - only
the preflight status is decided. Still one request per connection, no TLS, no body.
The shipping server still uses httplib.

No model was started.

## Reproduction

    python3 tools/compose-goose.py OUT/unit.goose \
        goose/server/{http-request,http-response,http-listen,route-table,api-key-policy,cors-policy}.goose
    <goose> check OUT/unit.goose
    # build the gate as in EXP978 and run it
