# EXP541: tools parsing (P7-08 slice 5, part 31)

## Hypothesis

`common_chat_tools_parse_oaicompat` validates and extracts: `null` means no
tools, the input must be an array, every element must be an object with
`type == "function"` and a `function` object, and the function's `name` is
required while `description` defaults to `""` and `parameters` defaults to the
empty object `{}`. All of that is expressible with a small JSON scanner that
records value spans, so nested `parameters` text can be spliced verbatim instead
of re-serialised.

That held. The runtime gained a span-tracking scanner (`js_ws`, `js_string_end`,
`js_value`, `js_member`) and `chat_tools_parse`, which returns the tool count or
a negative code for each rejection class. The harness compares accept/reject with
the native helper and, for accepted inputs, the extracted name, description and
the `parameters` structure.

The existing JSON parser module (`goose/json/json-parser.goose`) was not reused:
it is a standalone module whose helpers (`app`, `K_*`) clash with the runtime's
AST constants, and merging would need systematic renaming.

## Gate

```
FG541_RUNTIME_TOOLPARSE cases=471 values=832 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=471 values=832 failures=0
```

Eleven inputs: empty array, one tool with parameters, one with a description, two
tools, `null`, an object instead of an array, a missing type, an unsupported
type, a missing function, a missing name and malformed JSON.

Mutations: empty array treated as malformed, object accepted as the array, wrong
type name or type length, missing type/name accepted, description or name keeping
their opening quote, default parameters as an array, semicolon separator, a
closed array reporting no tools, and the member walk stopping after the first key.

## Limitations

- Names and descriptions are taken as the raw bytes between the quotes: escape
  sequences are not decoded, so `"a\"b"` would be compared verbatim.
- The rejection *texts* are not gated: the native message ends with
  `tools = <pretty dump>`, which would need a JSON pretty-printer; only the
  accept/reject decision is compared.
- The scanner accepts JSON numbers loosely (digit runs, optional fraction and
  exponent) and does not validate string escapes.
