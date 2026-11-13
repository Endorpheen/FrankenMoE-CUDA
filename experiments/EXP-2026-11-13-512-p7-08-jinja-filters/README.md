# EXP512: Jinja filters, part 1 (P7-08 runtime slice 5)

First filter slice: expression filters with dispatch, aliases, string coercion
and four text filters. Tests (`is ...`) and the remaining filters are later
parts.

## What was ported

`source/runtime.goose`, from `common/jinja/runtime.cpp` (`apply_filter`,
`filter_expression`) and the string built-ins in `value.cpp`:

| piece | rule |
|---|---|
| dispatch | a filter is an identifier or a call; the callee must be an identifier (`Filter callee must be an identifier`), arguments are evaluated in order |
| aliases | `count` becomes `length`; `trim` is routed like the native alias |
| coercion | `capitalize`/`lower`/`upper` coerce a non-string operand through `as_string`, except `undefined`; a negative conversion length aborts instead of being used as a slice bound |
| `upper` / `lower` | byte-wise ASCII case mapping, like the native `::toupper`/`::tolower` |
| `capitalize` | first byte upper, the rest lower |
| `length` | byte length of a string |
| errors | `Unknown (built-in) filter '<name>' for type <Type>` |

The output buffer is used as scratch for the transformed bytes and the string
cursor is rewound, so no fixed-size local buffer is needed.

## Gate

`tests/stmt_fixture.cpp` parses each template with the native lexer and parser,
executes it with the native runtime as the oracle and compares the rendered
output or the exact exception text.

| corpus | cases |
|---|---|
| upper, lower, capitalize, length (alias `count`), coercion of an integer, chained filters, unknown filter | 7 |
| total | **88 cases, 176 values, 0 failures** (the 81 of EXP510 plus 7) |
| mutations | **5/5** in two halves (upper/lower swap, capitalize first byte, alias removal, coercion removal, length off-by-one) |

## Not ported

- `strip`/`trim`, `replace`, `title`, `wordcount` and the container filters.
  `strip` was implemented but hung on a whitespace input in the fixture; the
  branch was removed so a call cannot hang and reports the unknown-filter error
  instead. The hang is not diagnosed yet and is recorded as the next filter
  step together with `replace`.
- Tests (`x is defined`, `x is none`, ...) are still native.

## Scope

P7-08 stays PARTIAL: filters are partially ported, tests, function values,
string marking/caps and the chat layer remain.
