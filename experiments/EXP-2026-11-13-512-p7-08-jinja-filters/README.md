# EXP512: Jinja filters, part 1 (P7-08 runtime slice 5)

Filter slice: expression filters with dispatch, aliases, string coercion, the
text filters and container lengths. Tests (`is ...`) and the remaining filters
(`title`, `wordcount`, the container-specific ones) are later parts.

## What was ported

`source/runtime.goose`, from `common/jinja/runtime.cpp` (`apply_filter`,
`filter_expression`) and the built-ins in `value.cpp` / `string.cpp`:

| piece | rule |
|---|---|
| dispatch | a filter is an identifier or a call; the callee must be an identifier (`Filter callee must be an identifier`), arguments are evaluated in order |
| aliases | `count` becomes `length`; `trim` is routed like the native alias |
| coercion | `capitalize`/`lower`/`upper` coerce a non-string operand through `as_string`, except `undefined`; a negative conversion length aborts instead of being used as a slice bound |
| `upper` / `lower` | byte-wise ASCII case mapping, like the native `::toupper`/`::tolower` |
| `capitalize` | first byte upper, the rest lower |
| `length` | byte length of a string, `vc` count of an array/tuple, `vkc` key count of an object |
| `strip` | two pointers trim ASCII whitespace on both sides, or the bytes of the `chars` argument when it is given |
| `replace` | left-to-right scan; a non-empty pattern is consumed by `pat.len`, an empty pattern inserts `new` before every byte and after the last one, like the native loop |
| errors | `Unknown (built-in) filter '<name>' for type <Type>` |

The output buffer is used as scratch for the transformed bytes and the string
cursor is rewound, so no fixed-size local buffer is needed.

## Gate

`tests/stmt_fixture.cpp` parses each template with the native lexer and parser,
executes it with the native runtime as the oracle and compares the rendered
output or the exact exception text.

| corpus | cases |
|---|---|
| upper, lower, capitalize, string length (alias `count`), coercion of an integer, chained filters, unknown filter | 7 |
| strip (whitespace, tabs, custom `chars`, all-whitespace), replace (single byte, multi-byte pattern, empty pattern, multi-byte replacement) | 8 |
| array/tuple/object length | 3 |
| total | **99 cases, 198 values, 0 failures** (the 81 of EXP510 plus 18) |
| mutations | **10/10** in two halves |

Mutations: upper/lower swap, capitalize first byte, `count` alias removal,
coercion removal, string length off-by-one, strip left side disabled, strip
`chars` ignored, replace trailing insert of an empty pattern dropped, replace
advance by `pat.len - 1`, object length off-by-one.

## History

Part 1 (commit `50ad6290`) shipped 88 cases and removed the `strip` branch
because the fixture hung on a whitespace input. The hang was **not** in the trim
logic: a standalone Goose probe (`/tmp/opencode/strip`, two-pointer trim, same
`-O0` build) returned `len=2 text=[hi]` at once. The fixture run was reading a
source file left mutated by an earlier killed mutation run (`ascii_upper` had
been rewritten to `ascii_lower` in all three branches), so the real defect was
the harness, not `strip`. The branch was restored with the output buffer as
scratch and the case set extended as part 2.

## Not ported

- `title`, `wordcount`, `string`/`mark`-style filters and the container filters
  (`map`, `select`, `sort`, `join`, `first`, `last`, `unique`, ...).
- Tests (`x is defined`, `x is none`, ...) are still native.

## Scope

P7-08 stays PARTIAL: filters are partially ported, tests, function values,
string marking/caps and the chat layer remain.
