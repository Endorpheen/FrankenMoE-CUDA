# EXP513: Jinja filters, part 2 (P7-08 runtime slice 5, filters)

## Hypothesis

The remaining text filters (`title`, `lstrip`, `rstrip`), the array accessors
(`first`, `last`) and the delimiter form of `join` need no new value shapes or
runtime services: they are byte loops over `vstr`, element reads through
`vcs`/`vcc` and the existing `as_string` conversion. A second hypothesis: the
native runtime answers several filters on an `undefined` operand with an empty
value instead of raising, so a differential gate must force that table to exist.

Both hypotheses hold.

## Native semantics (common/jinja)

| filter | rule |
|---|---|
| `title` | operand is coerced to a string by `apply_filter` (identifier form only); `titlecase` keeps whitespace bytes, uppercases the first non-space after whitespace and lowercases the rest |
| `lstrip` / `rstrip` | `strip(left, right)` with one side; no coercion in `apply_filter`, so a non-string operand is an unknown-filter error; `chars` replaces the whitespace set |
| `first` / `last` | array built-ins: first/last element, `undefined` for an empty array |
| `join` | array built-in: delimiter from the positional or `d=` keyword argument, `""` when absent; elements must be string/int/float, otherwise `join() can only join arrays of strings or numerics` |
| `wordcount` | only in the undefined built-in table, returns `0`; the string type has no `wordcount`, so a string operand is an unknown-filter error |
| `undefined` operand | `capitalize`, `first`, `join`, `last`, `length`, `lower`, `replace`, `strip`, `title`, `upper`, `wordcount` return an empty value (`""`, `0` or `undefined`) instead of raising |

## Implementation

`source/runtime.goose`:

- `title`, `lstrip`, `rstrip`, `first`, `last`, `join` added to the existing
  dispatch; all of them write through the output-buffer scratch and rewind the
  cursor, so no fixed-size local buffer is added.
- `join` converts each element with `as_string_into` and rejects anything that
  is not a string, integer or float with the native message.
- The `undefined` table answers the eleven names above before any type check.
- Coercion is now applied only to the identifier form of a filter (`fcall`),
  matching `apply_filter`; `replace` was added to the coercion set, `lstrip` and
  `rstrip` are deliberately outside it.

Two defects surfaced during the work:

1. A shared `recursive fn` helper for the unknown-filter text made the whole
   gate hang on the first error case. Reverting the helper and inlining the same
   seven lines at each site restored normal execution. The helper was not
   recursive itself, but it called `fail_node` and was itself declared
   `recursive`; the generated code then looped on the error path.
2. A mutation that stopped `rstrip` from decrementing its right pointer was a
   non-terminating mutation, not a missed one. The harness handles a mutation
   timeout, but the outer shell timeout must stay above the harness limit or the
   source is left mutated.

## Gate

`tests/stmt_fixture.cpp` parses each template with the native lexer and parser,
executes it with the native runtime as the oracle and compares the rendered
output or the exact exception text. `FG_TRACE=1` prints every template to
stderr, which is how the two hangs above were located.

| corpus | cases |
|---|---|
| `title` (mixed case, double space, integer coercion) | 3 |
| `lstrip` / `rstrip` (whitespace, custom chars, non-string error) | 5 |
| `first` / `last` (array, tuple, empty array, undefined, probing the value) | 6 |
| `join` (delimiter, no delimiter, empty array, mixed types) | 4 |
| `undefined` operand (`title`, `length`, `first`, equality probe) | 4 |
| string `wordcount` error | 1 |
| total | **122 cases, 244 values, 0 failures** (the 99 of EXP512 plus 23) |
| mutations | **12/12** in two halves |

Mutations: title stops after the first word; title uppercases the rest; lstrip
switches side; rstrip skips one byte too many; `chars` ignored; `first` returns
the last element; `last` returns the first; empty container returns a value
instead of `undefined`; join drops the last element; join ignores the delimiter;
undefined length is one; undefined `first` returns `""` instead of `undefined`.

## Not ported

- `truncate`, `format`, `indent`, `split`/`rsplit`, `startswith`/`endswith`,
  `int`/`float`/`string`/`safe`/`tojson`, `slice`, `sum` and the container
  filters (`map`, `select`, `sort`, `unique`, `dictsort`, `items`, `keys`,
  `values`, `get`), the `attribute` form of `join`, and tests (`is ...`).
- `string`-type `join` raises `String join builtin not implemented` natively;
  this slice does not reproduce it because the array form was the target.

## Scope

P7-08 stays PARTIAL: filters are partially ported, tests, function values,
string marking/caps and the chat layer remain.
