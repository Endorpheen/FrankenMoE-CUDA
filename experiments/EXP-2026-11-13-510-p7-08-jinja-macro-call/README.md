# EXP510: Jinja macro definition, positional parameters and calls (P7-08 runtime slice 4, parts 1-2a)

The macro/call slice so far: `macro_statement` defines a function value, calls
invoke it and capture the rendered body, positional parameters bind by index and
parameters with defaults fall back to the default expression. Keyword arguments
and the `call`/`caller` block are still open and are rejected explicitly.

## What was ported

`source/runtime.goose`, from `common/jinja/runtime.cpp`
(`macro_statement::execute_impl`, `call_expression::execute_impl`):

| piece | rule |
|---|---|
| macro definition | the name is bound in the current context to a function value that stores the macro node; the definition renders nothing |
| call | the callee is evaluated first, then checked: a non-function raises `Callee is not a function: got <Type>` (Integer, Undefined, ...) |
| invocation | a new scope, the macro body runs, its output is captured from the output buffer, the buffer is rewound and the text becomes a string value |
| nesting | a macro may call another macro defined earlier, and a macro body may use the loop/statement runtime |
| parameters | positional binding by index; extra arguments are ignored like the native runtime |
| defaults | a parameter written as `b=2` binds the argument when present and evaluates the default expression otherwise |
| missing argument | `Not enough arguments provided to '<name>'`, wrapped at the CallExpression site like native |
| keyword arguments | **rejected explicitly** with `Call keyword arguments are not supported yet` instead of binding a wrong value |
| `call`/`caller` | not ported yet and not accepted |

The evaluation path had to carry the output buffer: `eval_node`, `eval_binary`,
`eval_member` and `eval_call` now take `out`, and the context arrays are mutable
in that path because a macro body may `set` variables in its scope.

## Gate

`tests/stmt_fixture.cpp` parses with the native lexer and parser, executes with
the native runtime as the oracle and compares rendered output or the exact
exception text.

| corpus | cases |
|---|---|
| definition and call, text body, expression body, two calls, outer variable visible, `set` inside the body, nested macro call, definition alone | 8 |
| positional parameters, defaults, extra arguments, missing argument, nested call with an argument, parameter shadowing | 8 |
| macro body using `for` | 1 |
| non-function callee: integer literal call and an undefined name | 2 |
| total | **74 cases, 148 values, 0 failures** (the 56 cases of EXP509 plus 18) |
| mutations | **12/12** across the two parts: body execution, binding, stored node, capture, rewind, callee check, positional value, default value, parameter name |

The final `-O2` gate and ASan stay deferred for the same reason as EXP508/509.

## Limitations, stated explicitly

1. Keyword arguments in calls are rejected explicitly; the keyword node does
   not reach its own evaluator in the current AST encoding, so binding would
   otherwise read an Undefined argument.
2. `{% call m() %}...{% endcall %}` and the `caller` object are not ported.
3. Macro scoping is dynamic (the caller's current context), not the lexical
   context copy of the native runtime. Top-level macros behave identically; a
   macro defined inside a nested scope that captures shadowed variables can
   differ. This is recorded as a known difference, not hidden.

P7-08 stays PARTIAL: parameters/caller, filters and tests, function values in
the general sense, string marking/caps and the chat layer remain.
