# EXP507: Jinja expression evaluation in Goose (P7-08 runtime slice 1)

First part of the runtime slice: evaluating expressions and operators over the
flat AST from EXP503. Statements, filters/tests, member/call expressions and
function values are the next parts.

## What was ported

`source/runtime.goose`, from `runtime.cpp` (`statement::execute`,
`identifier::execute_impl`, `binary_expression::execute_impl`,
`unary_expression::execute_impl`):

| piece | rule |
|---|---|
| literals | integer, float and string nodes become values |
| identifiers | looked up in the context; a missing name becomes `Undefined` with the name as its hint |
| `and` / `or` | short-circuit, returning the deciding operand unchanged |
| `==` / `!=` | the equivalence relation from EXP504 (numeric across int/float/bool, structural for strings and hashable containers) |
| undefined / none | `in`/`not in` against undefined; `+`/`~` concatenate null/undefined with a string; otherwise the exact `Cannot perform operation ... on undefined values` and `Cannot perform operation on null values` |
| numbers | `+ - *` int when both operands are int, float when either is; `/` always float; `%` by `fmod`; `< > >= <=` |
| arrays | `+` concatenates; `x in array` and `not in` |
| strings | `+`/`~` concatenate (either side may be a non-string, converted with `as_string`); `*` repeats by an int; `in` is a substring test |
| objects | `key in object` with the unhashable-key error |
| errors | the native `statement::execute` wrapper: `\nError executing <Type> at position <pos>: <msg>`, applied once at the failing node |

Membership follows the native `test_is_in` texts, including
`'in' test expects args[1] as string when args[0] is string, got args[1] as
<Type>` and `'in' test expects iterable as first argument, got <Type>`.

## Gate

`tests/expr_fixture.cpp` parses `{{ expr }}` natively, executes the first body
node with a context, and compares the result (JSON) or the exact exception text
with the port, using the same context values and AST.

| corpus | cases |
|---|---|
| literals and identifiers (including missing names), unary `- + not`, arithmetic on int/int, int/float and float/float, division and modulo signs, comparisons, equality across int/float/bool/none/undefined/arrays | 54 |
| string concat/repeat/membership, array concat and membership, object membership, null/undefined concatenation and errors, `and`/`or` results, wrong-operand errors with the wrapper text | 27 |
| total | **81 cases, 162 values, 0 failures** |
| final gate | `-O2`, same 81/162 |
| mutations | **14/14** in two halves of seven (and/or results, strict-vs-equivalent equality, float arithmetic type, division type, modulo, comparison, string concat, repetition count, missing identifier type, null concatenation, undefined message, unary float type, wrapper position) |

ASan+UBSan is deferred: instrumenting the full generated module costs hours and
does not affect this correctness conclusion.

## Build loop

`build_fixture.sh [mutate|final]` uses `-O0` plus a cached fixture object for
mutations and `-O2` for the closing gate. Measured here:

| step | mutate | final |
|---|---|---|
| goose | 13.1 s | 12.6 s |
| gcc | 5.3 s (`-O0`) | 249.1 s (`-O2`) |
| fixture | cached / 3.2 s rebuilt | cached |
| link | 0.13 s | 0.11 s |
| run | 0.09 s | 0.08 s |

## Scope

P7-08 stays PARTIAL. The runtime slice has just started: expressions and
operators are Goose; statements (`if`/`for`/`set`/`macro`/`call`), filters and
tests, member/call expressions and function values are still native, then
string marking/caps and the chat layer.
