# EXP538: media content shape (P7-08 slice 5, part 28)

## Hypothesis

`common_chat_msg::to_json_oaicompat` chooses the content shape by a simple rule:
when the message has typed parts and any of them is a `media_marker`
(`contains_media`), the content is emitted as one joined string; otherwise the
content is an array of `{type, text}` parts; with no parts at all it is `""`. The
join uses the same rules as `concat_content_parts`. The native test helper
`common_chat_msgs_to_json_oaicompat(msgs, false)` builds caps with both content
shapes enabled, so the normalizer is a no-op and the helper exposes exactly this
shape choice — a usable oracle for both branches.

That held. `chat_content_json` implements the media detection, the joined-string
branch (reusing the concat rules) and the array branch, and the harness compares
its JSON structurally with the helper's content value.

## Rules captured

| condition | content shape |
|---|---|
| no parts | `""` |
| parts, no media marker | array of `{type, text}` in message order (unknown types included) |
| parts with a media marker | joined string: newline between text parts, none around media markers, unknown types skipped |

## Gate

```
FG538_RUNTIME_MEDIA cases=445 values=806 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=445 values=806 failures=0
```

Nine cases: two texts, text/media/text, media only, a single text, an unknown
type only, text/unknown/text, text/media/unknown/text (the case that makes the
join's skip path observable), no parts, and a text containing a newline.

Mutations: media detection inverted or looking for `text`, media not suppressing
or getting a newline, the media flag staying set, unknown parts skipping the next
part, empty parts producing an array, the array type key renamed, the separator a
semicolon, the joined string unquoted, the array losing its last part, and the
array text unescaped.

## Not ported

- The rest of `to_json_oaicompat` (reasoning_content, name, tool_call_id,
  tool_calls, id formatting) is exercised by the earlier tool probes only through
  usage flags, not as a full message serialization.
- The typed direction (no public oracle), tool schemas/grammars, the autoparser,
  string marking/caps, strftime_now and model parsers.
