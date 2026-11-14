# EXP565: JSON escaping in the value model (P7-08 adjacent, P7-09 fix)

## Finding

`value.goose` had a broken `json_string`: `0x08`/`0x0c` were written as raw
bytes and `\n`/`\r`/`\t` as raw control bytes, because a Goseliteral `"\n"` is a
newline, not a backslash-n. `tojson` in that module therefore produced invalid
JSON. The earlier EXP504 gate could not see it: its oracle built the expected
value with the same `\xNN` escaper, so the port was compared with its own bug.

Two escaped-module copies (`value-json.goose`, `builtins.goose`) also used
`\xNN`, which is not valid JSON; `builtins.goose` had no callers at all.

## Oracle

`value_to_json` from the native value model: raw UTF-8 (`Привет` stays
`d0 9f …`), control bytes as lowercase `\u00XX`, `0x7f` raw, `\b`/`\f` as
escapes, invalid UTF-8 copied without throwing. A probe records the bytes.

## Port

`json_string` now writes `\b \f \n \r \t`, `\u00XX` for other control bytes,
and raw bytes above 0x7e. `esc` in `value-json.goose` delegates to it.
`builtins.goose` is not changed: its `esc` has no callers, and it is not part of
this slice.

## Gate

```
value.goose       UNI3_VALUES cases=285 values=285 failures=0
value-json.goose  UNI3_VALUES cases=285 values=285 failures=0
mutations 7/7 caught (range 0:7)
mutations 7/7 caught (range 7:14)
```

285 specs per module, including Cyrillic, emoji, `0x01/0x0b`, `0x7f`,
`0x08/0x0c`, `0x1f`, CR, tab, quote+backslash, three invalid UTF-8 forms and two
nested structures. On the base version 12 of 285 fail, exactly the new specs.

## Integration

Copy `source/value.goose` and `source/value-json.goose` over `goose/json/`.
No exported signature changes: `value_info`, `value_cmp`, `value_json`,
`builtin_names` (only partially applicable here), `builtin_call`.

## Still open

- `esc` in `builtins.goose` is dead code and should be removed in a cleanup.
- `chat_message_full_json` and `chat_content_json` keep the spaced writer format;
  that is a formatting contract, not escaping.
- `chat_tools_json` inserts `parameters` verbatim; native parses it.
