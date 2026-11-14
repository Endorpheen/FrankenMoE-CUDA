# EXP540: tool schema serialization (P7-08 slice 5, part 30)

## Hypothesis

`common_chat_tools_to_json_oaicompat` returns `null` for an empty tool list and
otherwise one `{type: "function", function: {name, description, parameters}}`
object per tool, where `parameters` is the parsed JSON text of the schema.
`common_chat_tool_parameters` returns that value, or falls back to
`{type: "object", properties: {}}` when it is missing, null or an empty object.
Both are public, so both can be gated against the helpers directly.

That held. `chat_tools_json` splices the parameters text verbatim (the native
code parses it, and a valid schema text round-trips through the same JSON), and
`chat_tool_parameters_json` implements the fallback. The harness compares both
structurally with the public helpers.

## Rules captured

| piece | rule |
|---|---|
| empty list | `null` (not `[]`) |
| tool | `{type: "function", function: {name, description, parameters}}` |
| parameters | the schema JSON itself, not a string |
| fallback | missing, `null` or `{}` becomes `{type: "object", properties: {}}` |

## Gate

```
FG540_RUNTIME_TOOLSJSON cases=460 values=821 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=460 values=821 failures=0
```

Four tool-array cases (empty, one tool, empty description, two tools) and four
parameter cases (absent, `null`, `{}`, a real schema).

Mutations: empty list as `[]`, renamed `type`/`name`/`description`/`parameters`
keys, quoted parameters, the fallback ignoring `{}`, never triggering, wrong type,
an extra field, a semicolon separator and an unclosed tool object.

## Limitations

- The parameters text is spliced, not parsed: a whitespace variant such as
  `{ }` or `null ` is not recognised as the fallback, and invalid JSON (which
  makes the native helper throw) is out of scope. The fixture uses canonical
  texts.
- Tool argument generation, `common_chat_tools_parse_oaicompat` (the reverse
  direction), grammars, the autoparser, string marking/caps, strftime_now, the
  typed direction and model parsers remain native.
