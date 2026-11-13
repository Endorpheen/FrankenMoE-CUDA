# EXP450: request error policy in Goose (P8-02 third slice)

Malformed requests do not have a dedicated handler: every endpoint is wrapped by
`ex_wrapper`, which turns exceptions into OpenAI-shaped error payloads. This
experiment moves that policy into Goose.

## What was ported

`source/errors.goose`, transcribed from `server.cpp` (`ex_wrapper`) and
`server-common.cpp` (`format_error_response`):

1. **Exception classification**: `std::invalid_argument` and `common_json_error`
   become `INVALID_REQUEST`; any other `std::exception` and the catch-all become
   `SERVER`.
2. **Error type to HTTP code**: 400 invalid_request, 401 authentication,
   404 not_found, 500 server, 403 permission, 501 not_supported,
   503 unavailable, 400 exceed_context_size — exactly the native switch.
3. **Response status**: taken from the formatted payload's `code` with 500 as
   the fallback.

The error-type strings (`invalid_request_error`, ...) and the JSON document stay
native in this slice: the module returns the numeric type and the wrapper maps it
to the message. Goose does have byte-slice strings (`const u8[:]`), so moving the
message table is a later mechanical step, not a language limitation.

## Finding: there is no request Content-Type validation

The native server never inspects the request `Content-Type`. The only
`application/json` occurrences in `server.cpp`, `server-context.cpp`,
`server-http.cpp` and `server-common.cpp` are response headers (three sites);
the JSON body is parsed unconditionally with `json::parse(req.body)`. A body that
is not JSON therefore surfaces as `common_json_error` and becomes a 400 through
the path above.

Adding a Content-Type check would be new behaviour, not a port, so it is not
done. JSON document parsing itself stays with the vendored library, which is a
declared external dependency.

## Gate

| corpus | scope |
|---|---|
| exception classes 0..6 (named and unnamed classes) | classification |
| error types -2..10 (every enum member plus out-of-range) | code mapping |
| 15 payload codes x present/absent | status derivation |
| exception -> type -> code -> status chain for every class | end-to-end |
| total | **64 cases, 64 values, 0 failures** |
| ASan + UBSan host build | 0 failures, rc 0 |

Mutations 6/6 caught: JSON error becomes a server error (3 failures), not_found
404 -> 400 (1), unavailable 503 -> 500 (1), exceed-context 400 -> 500 (1), status
fallback 500 -> 200 (15), status ignores the payload code (16).

## Correction made while writing this slice

The first draft also modelled "POST endpoints require a JSON body". Reading the
handlers showed that is false: `post_props` takes `const server_http_req &` and
never reads the body, and `post_slots` reads it only after its feature checks.
The rule was removed rather than shipped with a wrong contract. The real
per-endpoint body/required-field table (`"prompt" is required`, `"messages"`,
`"input"`, `input_prefix`/`input_suffix`, ...) needs a per-handler audit and is
the next slice.

## Scope

P8-02 stays IN_PROGRESS. Still native:

- httplib transport, handler binding and `path_params` extraction;
- streaming (`server-stream.cpp`) and the task queue;
- per-endpoint required-field validation and body extraction;
- multipart, GCP AIP routes and the `/props` middleware payload.

## Reproduce

    bash experiments/EXP-2026-11-13-450-p8-02-request-errors/build_fixture.sh
    python3 experiments/EXP-2026-11-13-450-p8-02-request-errors/mutate.py
