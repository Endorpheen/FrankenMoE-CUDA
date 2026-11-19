# EXP-2026-11-18-979: the listener routes instead of answering ok

Status: routing gate PASS, 32 checks. The listener now consults the existing Goose
route table, so the answer depends on the request.

## Hypothesis fixed before changes

EXP978 proved the transport seam but answered `200 ok` to everything, which is a
stub, not a server. `goose/server/route-table.goose` already owns the mapping from
(method, path) to an endpoint category and was generated from the native
`server.cpp`. If the listener calls it, the status codes must follow the table:
a carried route is 200, an unknown path is 404, a method the table does not carry
is 405, and a query string must not change the routing.

## What was done

`hl_serve` now, after `rq_parse_line`:

1. maps the method slice to the code the table was generated with - GET 1, POST 2,
   DELETE 3, which is `METHODS` in `EXP-449/gen_routes.py`, the generator itself;
2. strips the query at the first `?`, because the table matches paths;
3. calls `route_id(code, path)`, which answers 0 for "no route";
4. answers 200 for a carried route, 404 for 0, 405 for a method the table does not
   carry, 400 for a request line that does not parse.

The method mapping is the one place where a wrong constant silently mis-routes, so it
is taken from the generator rather than guessed.

## Result

| request | answer |
| --- | --- |
| `GET /health` | `200 OK`, body `ok` |
| `GET /nope` | `404 Not Found`, no body |
| `GET /metrics?v=1` | `200 OK` - the query is stripped |
| `PUT /health` | `405 Method Not Allowed` |
| `GET /` (one space, no version) | `400 Bad Request` |
| gate | **32 checks, 0 failures** |

Every exchange still compares what the forked client received against the Goose
response buffer, so the assertions are about the socket.

## Not claimed

Still one request per connection, no TLS, no body reading, no API-key gate, no
category-specific bodies - `200` returns `ok` whatever the route. The route table
covers the paths it was generated with; the dynamic GCP compatibility routes are
excluded there and therefore here too.

Nothing in the shipping server calls this; it still uses httplib.

No model was started.

## Reproduction

    python3 tools/compose-goose.py OUT/unit.goose \
        goose/server/http-request.goose goose/server/http-response.goose \
        goose/server/http-listen.goose goose/server/route-table.goose
    <goose> check OUT/unit.goose
    # build the gate as in EXP978 and run it
