# EXP537: typed content concatenation (P7-08 slice 5, part 27)

## Hypothesis

`messages_inp_normalizer` converts content between two shapes depending on the
template caps. The string-content direction (`only_string`) joins typed parts
with `concat_content_parts`: a newline before every text part except the first
and except right after a media marker, media markers contribute their own text
without newlines, and unknown part types are skipped. A native oracle is
reachable for exactly that direction: `common_chat_msgs_to_json_oaicompat(msgs,
true)` builds caps with string content and typed content disabled, so the
normalizer concatenates the parts before returning the JSON.

That held. `chat_concat_parts` implements the join and is gated against the
normalized content string extracted from that helper's JSON.

## Rules captured

| rule | behaviour |
|---|---|
| text part after text | a newline separates them |
| text part after a media marker | no newline |
| media marker | no newline before it, its own text is appended |
| empty accumulated text | no leading newline |
| unknown part type | skipped, does not change the media state |

## Gate

```
FG537_RUNTIME_TYPED cases=436 values=797 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=436 values=797 failures=0
```

Eight cases: two texts, text/media/text, empty first text, unknown part between
texts, a single part, media first, two media markers, and a text containing its
own newline.

Mutations: media not suppressing the newline, no separation at all, newline
before media, the media flag never cleared, unknown parts skipping the next part
or acting like media, the newline rule requiring two bytes, the text slice using
the type bytes, a two-byte newline, inverted emission, the last part dropped, and
a newline after every part.

## Not ported

- The typed direction (`only_typed`: a string content becomes
  `[{type: "text", text: ...}]`). The test helper always constructs caps with
  string content enabled, so there is no public oracle for it; implementing it
  without a gate would be unverifiable.
- Wiring the normalization into message construction, and media-marker
  detection (`contains_media`), which the concat rule assumes.
- Tool schemas/grammars, the autoparser, string marking/caps, strftime_now and
  model parsers.
