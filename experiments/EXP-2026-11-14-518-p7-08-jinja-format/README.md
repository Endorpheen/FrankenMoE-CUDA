# EXP518: `format` and `indent` (P7-08 slice 5, part 8)

## Hypothesis

Both filters are byte scans behind the existing machinery: `format` substitutes
positional arguments into `{}` placeholders through `as_string_into`, and
`indent` prefixes every line with a width-derived prefix. No new value shape is
needed and the output buffer can carry the result.

Two native quirks must be reproduced rather than "fixed":

1. `format` supports only a bare `{}`; `{0}`, `{name}`, `{{` and a lone `{` are
   rejected with `format() only supports simple '{}' placeholders`.
2. `indent` decides "is this the first line" from `indented.empty()`, not from a
   line counter. When the first line is empty, the result is still empty after
   that line, so the next line is treated as the first one again and the leading
   newline disappears.

## Native semantics (common/jinja/value.cpp)

| piece | rule |
|---|---|
| `format` | input must be a string (the string table only; other types have no `format`); each `{}` takes the next positional argument converted with `as_string`; any other brace form raises the not-implemented message |
| `indent(width=4, first=false, blank=false)` | split the input on `\n` like `std::getline`; insert `\n` between emitted lines; indent a line when it is the "first" (output still empty, then `first`) or when it is non-empty or `blank`; a trailing `\n` in the input is re-appended, followed by the prefix when `blank` |
| width | integer → that many spaces; string → its bytes; anything else → four spaces |

## Plan

1. Implement `format` for string operands, letting other types fall through to
   the unknown-filter error as the native tables do.
2. Implement `indent` with the output-buffer layout: the prefix is built first,
   the content follows it, and the `empty` test becomes "content pointer equals
   its start".
3. Fixture cases: several placeholders, no placeholders, integer and float
   arguments, three brace-error forms, indent with integer and string width,
   `first`, `blank`, empty input, a trailing newline and an empty first line.
4. Gate, at least 12 mutations in two halves, docs, one commit.

## ACCEPT

- Gate green over the whole corpus.
- At least 12 mutations, all caught, baseline green after restore.
- The empty-first-line quirk and the trailing-newline rule are both exercised.
- No new fixed-size buffers.

## Result

```
FG518_RUNTIME_FMT cases=242 values=484 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=242 values=484 failures=0
```

One correction came out of the gate: the not-implemented brace error is raised
by `not_implemented_exception`, so the native text is the bare
`NotImplemented: format() only supports simple '{}' placeholders` with no
`Error executing ... at position N:` prefix. `fail_raw` was added for that case.

The `indent` quirk is reproduced and pinned: with `first=false` and an empty
first line, the leading newline disappears (`{{ '\nab'|indent(2) }}` renders
`ab`), because the native code tests `indented.empty()` rather than a line
counter. The mutation that replaces that test with a line counter is caught by
exactly that case.

Mutations: format always takes the first argument; format accepts any brace
pair; format keeps the closing brace; format writes a brace for every byte;
indent default width three; indent string width becomes spaces; indent first
flag inverted; indent ignores blank; indent always prefixes; indent drops the
trailing newline; indent drops the separator between lines; indent counts lines
instead of checking the output.

## Not ported

- `format` with `{0}`, `{name}`, `{{`/`}}`, format specs or conversions: the
  native implementation rejects all of them with the not-implemented message,
  which is what this slice reproduces.
- `format` with fewer arguments than placeholders raises our own
  `format() missing argument`; the native `get_pos` message differs and is not
  exercised by the fixture.
- `indent` with a negative or huge integer width: the native `std::string`
  constructor throws; a negative width would be unsafe here and is not covered.
- The container filters and `join` with `attribute` remain native.
