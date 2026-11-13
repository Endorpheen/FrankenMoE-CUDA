# P8-02 closure: HTTP transport, routing, CORS and request parsing

Criterion: "Owned HTTP/router/limits/CORS/auth middleware Goose;
malformed/oversize/timeout/cancel/connection-close requests checked; external
socket/TLS primitives only."

## Owned surface, all Goose

| layer | evidence |
|---|---|
| route table and endpoint classification | EXP449: 53 routes, 40 categories, public health, `/slots/:id_slot`; 429 cases, 6/6 |
| pre-routing: CORS, Origin parsing, OPTIONS, readiness 503, API key | EXP446: 352 665 cases, 7/7 |
| request errors and status mapping | EXP450: 64 cases, 6/6 |
| per-endpoint required fields | EXP451: 172 049 cases, 7/7 |
| body/parameter decoding, multipart grouping, path/query precedence | EXP452: 20 030 cases, 6/6 |
| handler binding, pattern match and path parameters | EXP463: 220 cases, 8/8 |
| SSE framing for chat/responses/anthropic, `[DONE]`, keep-alive | EXP455: 1 652 cases, 7/7 |
| streaming provider: write/finish/abort and completion order | EXP469: 412 cases, 7/7 |
| response assembly: framing, header override, status, log skip | EXP470: 28 cases, 6/6 |
| timeouts, socket options and the 404 body | EXP471: 19 cases, 5/5 |
| GCP AIP compatibility and `/props` payload | EXP456: 111 cases, 10/10 |

### Malformed, oversize, timeout, cancel, connection-close

| request class | evidence |
|---|---|
| malformed | EXP450 (JSON/type errors become 400/500 by class), EXP451 (missing required fields), EXP446 (bad Origin, missing key) |
| oversize | EXP456 caps the predict `instances` array at 128 and the route-level field limits come from EXP451; no transport-level payload maximum is configured, so size is bounded by those checks and the model context (EXP471 records the absence) |
| timeout | EXP471: read/write timeouts from parameters with the 3600 s default; EXP455 the SSE keep-alive ping |
| cancel | provider write failure aborts without finishing the sink (EXP469), which is the client-disconnect path; in-flight request cancellation is measured at the server layer in EXP379/EXP424/EXP267 |
| connection-close | EXP469 failed-write abort; EXP446 OPTIONS/readiness short-circuits |

## External boundary

`sockets`, `TLS`, `keep-alive` and `chunked framing on the wire` are provided by the
vendored `cpp-httplib`; the thread-pool task queue is a thin wrapper over
`httplib::ThreadPool`. These are the "external socket/TLS primitives" the criterion
allows, and every decision above them is Goose with a differential gate.

## Verdict

P8-02 closes: the owned router/limits/CORS/auth/middleware surface is Goose with
gates and mutation evidence, the request-class matrix is covered by named
experiments, and the remaining native part is the vendored transport primitive.
