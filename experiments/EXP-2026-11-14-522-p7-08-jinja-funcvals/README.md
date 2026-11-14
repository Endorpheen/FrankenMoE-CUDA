# EXP522: function values (P7-08 slice 5, part 12)

## Hypothesis

The runtime already stores macros as `V_FUNC` values holding a macro node id and
`invoke_func` binds parameters from the call-site AST, so a macro value can be
stored in a variable and passed as an argument without new machinery. What is
missing is the other source of function values in the native tree: an identifier
that is not in the context but exists in `global_builtins` resolves to a bound
function value. If that is added for a small, well-defined subset, function
values become first class in both directions.

Both parts held: macro values already worked, and the builtin path needed only a
marker plus a dispatcher.

## Native semantics (common/jinja/runtime.cpp, value.cpp)

| piece | rule |
|---|---|
| identifier | a name missing from the context but present in `global_builtins` returns `value_func` bound to that handler; otherwise `undefined` with the name as a hint |
| `range(start, stop, step)` | 1–3 integer arguments; one argument means `0..n`; a zero step raises `range() step argument must not be zero`; ascending while `i < stop`, descending while `i > stop` |
| `raise_exception(msg)` | raises `Jinja Exception: <msg>` |
| macro value | a macro name is a function value; storing it and calling it later must work |
| `is callable` | true for any function value |
| `is defined` | a builtin identifier is defined |

## Plan

1. Represent a builtin function value as `V_FUNC` with marker `-1` and the
   builtin id in the string-offset field; identifiers resolve `range` and
   `raise_exception` after the context lookup fails.
2. Dispatch builtin values in `eval_call` before `invoke_func`, evaluating the
   call arguments in the caller context.
3. Implement `range` and `raise_exception` with the native texts.
4. Fixture cases: three range forms, empty range, zero step, builtin-as-value,
   `is callable`/`is defined`, the error from a stored `raise_exception`, a macro
   stored in a variable, a macro passed to another macro, `for` over a range and
   `length` of a range.
5. Gate, at least 12 mutations in two halves, docs, one commit.

## ACCEPT

- Gate green over the whole corpus.
- At least 12 mutations, all caught, baseline green after restore.
- A macro value passed as an argument is callable inside the callee.
- No new fixed-size buffers.

## Not ported

- `dict`, `namespace`, `strftime_now` and the remaining `global_builtins`
  entries; only `range` and `raise_exception` are values here.
- Bound method values (`value_func` with `arg0`) from attribute access such as
  `obj.method`; the attribute path still returns plain values.
- `{% call %}` with a builtin function value.
