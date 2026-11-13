# EXP503: Jinja parser and AST shapes in Goose (P7-08 second slice)

The token stream from EXP502 becomes a tree. The native parser builds a class
hierarchy with virtual methods; this slice ports the decision layer and the node
shapes, and keeps execution (the virtual `execute` methods) for the runtime
slice.

## What was ported

`source/parser.goose`, from `parse_from_tokens`:

| decision | rule |
|---|---|
| statements | `set` (assignment and multiline), `if`/`elif`/`else`/`endif`, `for`/`else`/`endfor`, `break`, `continue`, `macro`/`endmacro`, `call`/`endcall` with optional caller arguments, `filter`/`endfilter`, `generation`/`endgeneration` as no-ops |
| precedence | ternary/select, `or`, `and`, `not`, comparison (`< <= == != > >= in "not in"`), additive `+ - ~`, multiplicative `* / %`, `is` tests (`is not`, bare argument, call form), filters `|`, unary, call/member, primary |
| postfix | member `.x`, computed `[...]`, slices `[a:b:c]`, blank `[]`, call `(...)`, chained calls/members |
| arguments | positional, `name=value` keyword, `*expr` spread, trailing comma allowed (`f(1,)` is valid; `(1,)` is not) |
| literals | integers with an optional sign (the lexer emits the sign inside a numeric token), floats by `strtod`, adjacent string concatenation, arrays, tuples, objects |
| positions | every node carries the token position `mk_stmt` would use: statement names, operator tokens, the `else` of a ternary, the first `.`/`[` of a member chain |
| errors | `parser: <msg>` plus the source view for expected-token errors, and the plain messages (`Unknown statement: x`, `Expected 'in'`, `Unexpected token: x of type N`) byte for byte |

The tree is flat: kind, position, scalar fields, three direct children and two
linked child lists. Lists are (value, next) entries in a shared pool, because
nested nodes append to their own lists between a parent's appends.

## Gate

`tests/parser_fixture.cpp` tokenizes natively, encodes the token stream, parses
with the native `parse_from_tokens` and with the port, and compares:

- on success: a full tree dump (type, position, scalar fields, every child and
  child list) produced twice — once by walking the native class hierarchy with
  `dynamic_cast`, once from the flat arrays;
- on failure: the exact exception text, including the source view.

| corpus | cases |
|---|---|
| hand-written: every node kind, the whole precedence chain, member/slice/call forms, keyword and spread arguments, filters and tests, ternaries, all statements, comments, and 21 rejected templates | 100 |
| generated: 2 000 text/expression pairs, 100 nested parentheses, 500-term additive chain, 500 arguments, 1 000 if blocks | 5 |
| total | **105 cases, 210 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |
| mutations | **19/19** (ternary kind, negation, bare test argument, computed flag, slice order, keyword, spread, adjacent strings, float detection, literal sign, elif placement, for-else placement, set body, caller arguments, generation consumption, comment value, error value, caret column, filter binding) |

Two bugs were caught during the port: child lists were first stored as
contiguous pool regions, which nested nodes broke by appending in between (now
linked entries), and an empty list's head started at 0 instead of -1, which made
the dump recurse into node 0 forever. The gate also caught that numeric literals
can carry a leading sign from the lexer's unary handling, so integer parsing
must consume it.

## Scope

P7-08 stays PARTIAL: execution (`runtime.cpp`), values and built-ins
(`value.cpp`), input marking (`string.cpp`), capability checks (`caps.cpp`), the
chat layer and the per-model parsers are still native. Slice order:
`docs/goose-migration/P7-08-COVERAGE.md`.
