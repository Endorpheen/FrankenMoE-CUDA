# EXP508: Jinja statement runtime in Goose (P7-08 runtime slice 2)

Second part of the runtime slice: executing a parsed program and rendering its
output over the flat AST from EXP503. The expression evaluator, the value model
and the built-ins come from EXP507, EXP504-506.

## What was ported

`source/runtime.goose`, from `runtime.cpp` (`runtime::execute`,
`runtime::gather_string_parts`/`render_string_parts`, `program::execute_impl`,
`if_statement::execute_impl`, `set_statement::execute_impl`, `context::set_val`
and the output buffer):

| piece | rule |
|---|---|
| program | the root body runs statement by statement; execution stops on the first failure |
| output | a non-statement node is evaluated and rendered into the output buffer; strings are written byte for byte, other values through `as_string` |
| `if` / `elif` / `else` | the test goes through `as_bool`; a truthy test runs the body, otherwise the alternate; no scope is pushed (native behaviour) |
| `set name = expr` | the value is bound in the context; a later binding of the same name shadows the earlier one (last write wins) |
| `set a, b = expr` | the right side must be an array or tuple, its length must match the assignee count (`Too few items to unpack in set` / `Too many items to unpack in set`), every target must be an identifier (`Cannot unpack to non-identifier in set: <Node>`), then the items bind left to right |
| `{% set name %}...{% endset %}` | the block body renders into the output, the captured bytes become the string value, and the capture marker is rewound to the previous output end |
| errors | the native `statement::execute` wrapper `\nError executing <Type> at position <pos>: <msg>` at the failing node; `Cannot unpack non-iterable type in set: <Type>` and `Invalid LHS inside assignment expression` |
| `break` / `continue` | the signal is recorded (1 and 2) and stops the current list; the loop that consumes it is not ported yet |

The context keeps a name/value pair per binding, searched from the newest entry
backwards, so shadowing works without destroying the outer binding. A small
scope stack (`scope_push`/`scope_pop`) is present for the `for` slice; `if`
deliberately does not use it, which is what the native runtime does and what the
`{% if 1 %}{% set z = 7 %}{% endif %}{{ z }}` case checks.

## Gate

`tests/stmt_fixture.cpp` parses a template with the native lexer and parser,
executes it with the native `jinja::runtime` (the oracle), encodes the same
context and AST for the port, and compares the rendered output bytes or the
byte-exact exception text.

| corpus | cases |
|---|---|
| plain text, interpolated expressions, all five context values (array, float, bool, none, undefined), text with a newline | 9 |
| `set` (simple, chained, block capture, tuple unpack from a literal and from a context array, shadowing an existing name) | 6 |
| `if` (true, false/else, `elif`, on a context value, nested, set inside a branch, mixed with output) | 7 |
| rejected templates (tuple size too large, too small, too many targets, non-identifier target) | 4 |
| total | **27 cases, 54 values, 0 failures** |
| mutations | **12/12** in two halves of six |

The mutation set covers both `if` branches, the program loop bound, `set`
assignment, tuple unpack order, tuple size check, non-iterable tuple, block-set
output rewind, string rendering, context shadowing order, the error-wrapper
position and the identifier check in tuple targets. Two mutations are caught by
the binary dying (an out-of-bounds index and a runaway recursion) and ten by
gate mismatches; the harness now reports both as caught and prints the reason.

The final `-O2` gate is **deferred**: `gcc -O2` on the 8.7 MB generated module
did not finish inside the five-minute per-step limit (the run was killed at
295 s, exit 124). The correctness conclusion rests on the `-O0` gate above,
which is the same corpus and the same generated code; `-O2` only changes
optimisation. ASan+UBSan stays deferred for the same reason as EXP506/507.

## Build loop

`build_fixture.sh [mutate|final]` uses `-O0` plus a cached fixture object for
mutations and `-O2` for the closing gate. Measured here:

| step | mutate |
|---|---|
| goose | 15.3 s |
| gcc | 6.2 s (`-O0`) |
| fixture | cached |
| link | 0.11 s |
| run | 0.02 s |
| total | 21.6 s |

## Scope

P7-08 stays PARTIAL: program/if/set and the output path are Goose. `for` with
`loop.*` and break/continue consumption, `macro`/`call`, filters and tests,
member/call expressions and function values are still native, then string
marking/caps and the chat layer.
