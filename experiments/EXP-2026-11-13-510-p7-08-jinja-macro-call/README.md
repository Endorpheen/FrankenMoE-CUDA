# EXP510: Jinja macro definition and zero-argument calls (P7-08 runtime slice 4, part 1)

First part of the macro/call slice: `macro_statement` defines a function value
and a call expression invokes it, capturing the rendered body as a string.
Parameters, keyword arguments and the `call`/`caller` block are part 2.

## What was ported

`source/runtime.goose`, from `common/jinja/runtime.cpp`
(`macro_statement::execute_impl`, `call_expression::execute_impl`):

| piece | rule |
|---|---|
| macro definition | the name is bound in the current context to a function value that stores the macro node; the definition renders nothing |
| call | the callee is evaluated first, then checked: a non-function raises `Callee is not a function: got <Type>` (Integer, Undefined, ...) |
| invocation | a new scope, the macro body runs, its output is captured from the output buffer, the buffer is rewound and the text becomes a string value |
| nesting | a macro may call another macro defined earlier, and a macro body may use the loop/statement runtime |
| arguments | **not in this part**: a macro with parameters or a call with arguments fails explicitly with `Macro parameters are not supported yet` / `Call arguments are not supported yet` instead of silently ignoring them |

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
| macro body using `for` | 1 |
| non-function callee: integer literal call and an undefined name | 2 |
| total | **66 cases, 132 values, 0 failures** (the 56 cases of EXP509 plus 10) |
| mutations | **6/6** in two halves of three (body execution, binding, stored node, capture, rewind, callee check) |

The final `-O2` gate and ASan stay deferred for the same reason as EXP508/509.

## Limitations, stated explicitly

1. Macro parameters and defaults are not ported; such a macro fails at the call
   instead of producing a wrong result.
2. `{% call m() %}...{% endcall %}` and the `caller` object are not ported.
3. Keyword arguments are not ported.
4. Macro scoping is dynamic (the caller's current context), not the lexical
   context copy of the native runtime. Top-level macros behave identically; a
   macro defined inside a nested scope that captures shadowed variables can
   differ. This is recorded as a known difference, not hidden.

P7-08 stays PARTIAL: parameters/caller, filters and tests, function values in
the general sense, string marking/caps and the chat layer remain.
