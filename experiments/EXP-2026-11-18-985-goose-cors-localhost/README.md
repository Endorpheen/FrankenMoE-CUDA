# EXP-2026-11-18-985: the localhost CORS mode, wired

Status: gate PASS, 97 checks. The `cors_origins == "localhost"` mode no longer
refuses every origin.

## Hypothesis fixed before changes

EXP981 emitted CORS headers but passed `origin_local = 0`, so the localhost mode -
which reflects a local Origin and refuses a foreign one - always took the refusal
branch. The missing input is the Origin's host, and that parse already exists:
`http_parse_url` produces it and `cors_is_localhost` compares it. If the listener
joins the two, a local Origin must be reflected and a foreign one must get no header.

## What was done

Before calling `cors_allow_origin`, the listener runs `http_parse_url` on the Origin
value and passes the host span to `cors_is_localhost`. No second URL parser was
written: the decision is the one that already exists.

`url_parse` writes five spans - scheme, userinfo, password, **host** and path - so the
offset and length arrays are sized five and the host is index 3.

## Result

| request | policy | answer |
| --- | --- | --- |
| `Origin: http://localhost:8080` | `localhost`, credentials | the origin is reflected, `Allow-Credentials: true` |
| `Origin: http://127.0.0.1:3000` | `localhost`, no credentials | the origin is reflected |
| `Origin: http://evil.example` | `localhost`, credentials | **no** allow-origin header |
| gate | | **97 checks, 0 failures** |

Whole suite, re-run by `tools/verify-unit-gates.sh` on the current unit: typical
2535, top-n-sigma 17112, dry 2440, http-request 566, http-listen 97 - **22 750
checks, 0 failures**, unit 1158 globals and 0 duplicates.

## Two runtime bounds failures, each a real contract I had not read

The first attempt passed one-element arrays to `http_parse_url` and died with
`index 1 out of bounds`; the second, with three elements, died at `index 4`. Only
reading `url_parse` showed the five spans and which one is the host.

This is worth recording because it is the opposite of the usual failure here: the
typecheck was happy, the build was clean, and the contract was in the data layout
rather than in the signature. A Goose slice carries a length, and indexing past it is
a runtime abort - which at least is loud, unlike the write past a C array.

## Not claimed

CORS headers are still emitted only on the preflight, not on ordinary responses. One
request per connection is no longer true (EXP982/983), but there is still no TLS, no
body reading and no `Connection: close` handling. The shipping server still uses
httplib.

No model was started.

## Reproduction

    bash tools/verify-unit-gates.sh
    cat builds/verify-gates/run-http-listen.log
