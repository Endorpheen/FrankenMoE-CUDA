# EXP539: full message fields (P7-08 slice 5, part 29)

## Hypothesis

`common_chat_msg::to_json_oaicompat` emits `role` and `content` first, then the
optional `reasoning_content`, `name` and `tool_call_id` (each only when
non-empty), then `tool_calls` as an array of
`{type: "function", function: {name, arguments}}` with `id` appended only when
present. `arguments` stays a JSON *string* (the field is a `std::string`), not a
parsed object. The helper `common_chat_msgs_to_json_oaicompat(msgs, false)`
exposes this shape, so it can be gated structurally.

That held. `chat_message_full_json` emits the full object for a string-content
message and the harness compares it with the helper's JSON over seven messages.

## Rules captured

| field | rule |
|---|---|
| order | role, content, reasoning_content, name, tool_call_id, tool_calls |
| empty optional fields | omitted |
| tool call | `{type: "function", function: {name, arguments}}`, `id` appended last when non-empty |
| arguments | JSON string, escaped like any string |

## Gate

```
FG539_RUNTIME_FIELDS cases=452 values=813 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=452 values=813 failures=0
```

Seven messages: user content only; with reasoning; with name and tool_call_id;
an assistant with one call without id; the same with an id; two calls; and a
content containing a newline and a quote.

Mutations: empty reasoning or empty ids emitted, renamed keys (`tool_call_id`,
`id`, `content`, `role`, `reasoning_content`), the call `type` dropped, arguments
unescaped, calls not separated, and wrong closing delimiters.

## Not ported

- Parts-based content in this serializer (covered separately by EXP537/EXP538).
- `tool_call.arguments` validation/parsing and the generated-id path (commented
  out natively).
- Tool schemas/grammars, the autoparser, string marking/caps, strftime_now, the
  typed direction and model parsers.
