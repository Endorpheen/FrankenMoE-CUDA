# EXP542: escape decoding and tools error texts (P7-08 slice 5, part 32)

## Hypothesis

EXP541 recorded two gaps: `name`/`description` were copied as raw bytes between
the quotes (escape sequences were not decoded), and the rejection texts were not
gated because they end with `tools = <pretty dump>`. Both are deterministic: the
native parser decodes string escapes, and the wrapper builds
`Failed to parse tools: <inner>; tools = <dump(2)>`.

That held. The scanner gained `js_decode` (`\"`, `\\`, `\/`, `\b`, `\f`, `\n`,
`\r`, `\t`, `\uXXXX` with surrogate pairs to UTF-8), `js_pretty` reproduces
nlohmann's `dump(2)` layout, `js_compact` reproduces plain `dump()`, and
`chat_tools_error` assembles the wrapper. `chat_tools_parse` now also reports the
failing span through a `fail` slice.

## The detail the first attempt got wrong

The inner text is not the input slice: it is `value.dump()`, i.e. the **compact**
form. An input written as `{"function": {"name": "x"}}` appears in the message as
`{"function":{"name":"x"}}`. The gate caught this immediately — three texts
differed only by spaces. A compact dumper was added next to the pretty one.

## Native texts confirmed by a probe

| input | inner text |
|---|---|
| `{}` | `Expected 'tools' to be an array, got {}` |
| `"x"` | `Expected 'tools' to be an array, got "x"` |
| no `type` | `Missing tool type: <compact element>` |
| `type:"tool"` | `Unsupported tool type: <compact element>` |
| no `function` | `Missing tool function: <compact element>` |
| empty `function` | `[json.exception.out_of_range.403] key 'name' not found` |

Suffix: `; tools = <dump(2)>` — two spaces per level, `": "` after keys, newline
between items.

## Gate

```
FG542_RUNTIME_TOOLMSG cases=476 values=842 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=476 values=842 failures=0
```

The 471 cases of EXP541 plus five decoded-name cases (`a\"b`, `a\nb`, `\u00e9`,
a surrogate pair, `a\/b`), and an exact text comparison for every input the
native side rejects with the wrapper prefix.

Mutations: escapes copied verbatim, `\n` decoded as a space, the low surrogate
dropped, a wrong two-byte UTF-8 lead, one-space indent, no colon space in pretty
output, an extra colon space in compact output, singular wrapper prefix, a
different missing-type text, a squeezed suffix and a different missing-name text.

## Limitations

- Malformed JSON fails earlier in the native path (a `parse_error` before the
  tools parser), so that text is not gated; only accept/reject is compared.
- Both dumpers copy scalar and key bytes verbatim from the input. For canonical
  JSON that equals nlohmann's output, but a differently escaped input (for
  example `\u0041`) could be re-emitted differently by the native side.
- `js_value` still validates numbers and escapes loosely.
