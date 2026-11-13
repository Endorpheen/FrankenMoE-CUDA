# EXP278: API compatibility suite on the accepted runtime

P8-04's criterion is a compatibility suite, not a port. This is the first pass, run against the accepted staging runtime
with the accepted argv and request.

## Checks and results

| check | expectation | result |
|---|---|---|
| non-stream completion | baseline `e68df05b...`, 128 tokens, finish `limit` | pass, `e68df05b4829865dcf8cd2424febfaded15b055f56fec36fefc002650ef8e7c9`, 128, `limit` |
| streaming the same request | concatenated content and token ids equal the non-stream answer | pass, 129 chunks, 128 tokens, equal |
| request with `stop: [" "]` | finish reason `word` | pass, `word` |
| invalid request (`prompt: 123`, `n_predict: "nonsense"`) | HTTP 4xx with an error body | pass, 400 with a message naming the prompt element |
| disconnect mid-stream, then a normal request | later request still served | pass |

Exit code is 0 only when every check passes; the suite writes `run/suite.json`.

## What this covers and what it does not

Covers the four shapes the criterion names for the completion path: stream versus non-stream equality on content and token
ids, finish reason for a length stop and for a word stop, an error response, and behaviour after a client disappears.

Does not cover yet: the chat endpoint and its `finish_reason` mapping, the responses API, token-usage accounting in the
response body, tool calls, and multi-client interleaving. Those are the next checks to add; the harness already has the
server lifecycle, streaming reader and error path they need.

## Why the baseline matters

The non-stream hash is the same `e68df05b` the whole migration uses as its reference, so this suite can be run unchanged
against any candidate build: a different hash means the API surface or the model path changed, not the test.
