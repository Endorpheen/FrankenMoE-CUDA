# EXP455: SSE framing in Goose (P8-02 eighth slice)

Streaming responses are assembled from a few fixed shapes: `data: <json>` for
chat chunks, `event: <name>` plus `data:` for responses and Anthropic, a
`data: [DONE]` terminator, and a `:\n\n` keep-alive ping. This experiment moves
the shape into Goose.

## What was ported

`source/sse.goose`, from `server-common.cpp` (`format_oai_sse`,
`format_oai_resp_sse`, `format_anthropic_sse`) and `server-context.cpp` (the
`[DONE]` terminator and the `:\n\n` ping):

| flavor | plan |
|---|---|
| OpenAI chat | `data: ` payload newline newline |
| OpenAI responses | `event: ` name newline `data: ` payload newline newline |
| Anthropic | the responses plan when the object has both `event` and `data`, otherwise the chat plan |
| terminator | `data: ` `[DONE]` newline newline |
| keep-alive | `:` newline newline |

The module returns the ordered step plan and, separately, the exact byte length
for a given payload and event-name length. The literals and the JSON
serialisation stay native; the wrapper expands the plan.

## Gate

`tests/sse_fixture.cpp` transcribes the native formatters into real strings and
expands the Goose plan with the same literals, then compares bytes, per-frame
length and the array total.

| corpus | cases |
|---|---|
| 5 flavors x event present/absent x 5 payloads (0, 2, 1, 9, 50 bytes) x 3 event names x item counts 0/1/3 | 1 645 |
| terminator plan shape and bytes pinned explicitly | 7 |
| total | **1 652 cases, 2 552 values, 0 failures** |
| ASan + UBSan (C and C++ objects) | 0 failures, rc 0 |

Mutations 7/7 caught: chat frame loses the trailing blank line (240 failures),
responses frame swaps event and data (40), Anthropic inverts the event branch
(240), terminator emits a payload step instead of `[DONE]` (242), keep-alive
sends a data prefix (240), event prefix measured as six bytes (270), newline
measured as two bytes (900).

## What stays native

- the literal bytes and `safe_json_to_str`, which is the JSON library boundary;
- the chunked content provider, the ping timer and the should-stop polling;
- the decision of *which* flavor an endpoint uses, which is part of the handler.

P8-02 stays IN_PROGRESS on the transport, handler binding, GCP AIP and `/props`.

## Reproduce

    bash experiments/EXP-2026-11-13-455-p8-02-sse-framing/build_fixture.sh
    python3 experiments/EXP-2026-11-13-455-p8-02-sse-framing/mutate.py
