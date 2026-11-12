# EXP451: per-endpoint required fields in Goose (P8-02 fourth slice)

Each POST handler enforces its own body contract. This experiment audits all of
them and moves the presence contract into Goose.

## Audited table

| category | required | native citation |
|---|---|---|
| 6 POST /models | `model` non-empty | server-models.cpp:2594 |
| 11 POST /completion | `prompt` | server-context.cpp:5192 |
| 12 POST /completions, /v1/completions | `prompt` | server-common.cpp:1159 |
| 13 POST /chat/completions | `messages` | server-common.cpp:1402 |
| 14 POST /v1/chat/completions/control | control id | server-context.cpp:5890 |
| 15 POST /responses | `input` | server-chat.cpp:8 |
| 16 POST /audio/transcriptions | multipart `file` | server-chat.cpp:670 |
| 19 POST /v1/messages | `messages` | server-chat.cpp:377 |
| 20 POST /infill | `input_prefix` + `input_suffix` | server-context.cpp:5783/5787 |
| 21 POST /embedding, /embeddings | `input` **or** `content` | server-context.cpp:6802 |
| 22 POST /v1/embeddings | `input` **or** `content` | server-context.cpp:6802 |
| 23 POST /rerank family | `query` + `documents`/`texts` | server-context.cpp:6308/6315 |
| 27 POST /apply-template | `messages` | server-common.cpp:1402 |
| 28 POST chat input_tokens | `messages` | server-common.cpp:1402 |
| 29 POST responses input_tokens | `input` | server-chat.cpp:8 |
| 30 POST /v1/messages/count_tokens | `messages` | server-chat.cpp:377 |
| 32 POST /lora-adapters | whole body is an array | server-context.cpp:6649 |

The other 23 categories have no presence requirement and are not invented here:
`/props` and `/slots` read no body at all, `/tokenize` and `/detokenize` treat
`content`/`tokens` as optional, `/tts`, `/systemone`, `/models/load` and
`/models/unload` read optional fields through `json_value`, and the remaining
endpoints validate field values (control action, `response_format`, `top_n`)
rather than presence.

## Module

`source/required.goose` exposes `missing(category, present_mask)` returning the
mask of missing required fields. Field bits: prompt 1, messages 2, input 4,
input_prefix 8, input_suffix 16, content 32, query 64, documents/texts 128,
model 256, control id 512, array body 1024, multipart file 2048. The wrapper maps
each bit to the native message text; the strings stay native.

## Gate

`audit.py` is both the contract and the oracle: it holds the table with the
native citation for every entry, **re-reads those native lines and fails the
build if any citation drifts**, then emits the fixture's expected table. The
fixture therefore compares Goose with the audited source, not with a second hand
copy.

| corpus | cases |
|---|---|
| all 40 categories x all 4096 field masks | 163 840 |
| audited table entries against an empty mask | 17 |
| any-of endpoints (embeddings) across all masks | 8 192 |
| total | **172 049 cases, 0 failures** |
| ASan + UBSan host build | 0 failures, rc 0 |

Mutations 7/7 caught: `/completion` loses prompt (2 049 failures), infill only
prefix (2 049), rerank drops documents (2 049), embeddings require both input and
content (8 192), chat requires prompt instead of messages (3 073), transcription
file bit becomes array-body (3 073), control id removed (2 049).

## Native quirk found

`/completion` reads `data.at("prompt")`, so a missing prompt is a JSON type error
→ 400. `/completions` instead throws `std::runtime_error("\"prompt\" is
required")`, which `ex_wrapper` classifies as a server error → **500** with the
message. The same missing field therefore returns different statuses on the two
routes. This is recorded; the contract module reports presence only and does not
normalise it.

## Scope

P8-02 stays IN_PROGRESS: httplib transport, handler binding and `path_params`,
streaming, multipart decoding itself, GCP AIP routes, `/props` payload and the
value-level validation stay native.

## Reproduce

    bash experiments/EXP-2026-11-13-451-p8-02-required-fields/build_fixture.sh
    python3 experiments/EXP-2026-11-13-451-p8-02-required-fields/mutate.py
