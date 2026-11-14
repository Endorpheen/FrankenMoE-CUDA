# EXP543: strict JSON validation and contains_media (P7-08 slice 5, part 33)

## Hypothesis

EXP542 recorded that `js_value` validated numbers and escapes loosely, so a
number such as `01`, `1.` or `1e` and an escape such as `\q` or `\u12` could
pass our scanner while `common_json::parse` rejects them. Tightening the scanner
to the RFC 8259 rules nlohmann implements makes accept/reject agree with the
native parser, and `contains_media` (the last unported branch of the message
shape) is a plain scan over part types.

That held. `js_string_end` now rejects raw control bytes, limits escapes to the
eight named ones, and requires four hex digits after `\u`. `js_value` requires
`-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?`, so leading zeros, a bare dot,
a fraction without digits and an exponent without digits are rejected.
`chat_contains_media` is exported and gated against
`common_chat_msg::contains_media()`.

## Rules captured

| piece | rule |
|---|---|
| string | raw bytes below 0x20 are invalid; escapes limited to `" \ / b f n r t u`; `\u` needs four hex digits |
| number | optional `-`, then `0` or a non-zero leading digit run, optional fraction with at least one digit, optional exponent with at least one digit |
| contains_media | true when any content part has type `media_marker` |

## Gate

```
FG543_RUNTIME_STRICT cases=494 values=860 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=494 values=860 failures=0
```

Twenty tools inputs (the nine of EXP542 plus `+1`, `01`, `1.`, `.5`, `1e`, `-`,
`\u12az`, a raw 0x01 byte, an unterminated string, a trailing comma and a valid
`-0`/`1.5e+2` control), and five `contains_media` cases (no parts, text only,
text+media, media first, unknown type only).

Mutations: raw control accepted, non-hex `\u`, a named escape advancing one byte,
`t` dropped from the escape set, leading zero accepted, fraction without digits,
exponent without digits, a lone minus accepted, a three-digit `\u` check,
`contains_media` matching `text`, always true, and `\u` not recognised.

## Limitations

- Whitespace outside the grammar is handled by `js_ws`; a byte-order mark or a
  non-UTF-8 byte inside a string is not specially diagnosed.
- The scanner validates but does not normalise numbers, so a value such as
  `-0` is kept verbatim in `parameters` (as the native side also keeps it).
- The typed-content direction, grammars/autoparser, string marking/caps,
  `strftime_now` and model parsers remain native.
