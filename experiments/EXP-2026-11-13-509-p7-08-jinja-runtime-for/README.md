# EXP509: Jinja `for`, `break`/`continue` and the loop object (P7-08 runtime slice 3)

Third part of the runtime slice: `for_statement`, its optional filter
(`select_expression`), `break`/`continue` consumption inside the loop, the
`loop` object and the member access the loop fields need. Expressions,
statements and the value model come from EXP504-508.

## What was ported

`source/runtime.goose`, from `common/jinja/runtime.cpp`
(`for_statement::execute_impl`, `break_statement`, `continue_statement`,
`member_expression::execute_impl`) and the `loop` object it builds:

| piece | rule |
|---|---|
| iterable | array, tuple or object; `undefined` means zero items, any other type is `Expected iterable or object type in for loop: got <Type>` |
| object iteration | keys and values are bound directly from the object's regions; a single loop variable receives the key, a tuple receives key and value |
| array iteration | the item itself; a tuple loop variable unpacks it |
| filter | `{% for x in items if cond %}` evaluates the condition per item with the loop variable bound in its own scope, before the loop object exists |
| scope | the loop body gets a fresh scope per iteration: a `set` inside the body does not survive the iteration or the loop |
| `break` / `continue` | the signal recorded by the statement is consumed by the loop: break stops the loop, continue moves to the next iteration; both are reset so an enclosing loop continues |
| `else` block | runs only when no iteration completed normally; an iteration ended by break or continue does not count, which reproduces the native quirk where a break on the first iteration still runs `else` |
| loop object | `index`, `index0`, `revindex`, `revindex0`, `first`, `last`, `length`, `previtem`, `nextitem`; the first/last missing neighbours are `Undefined` with the field name as the hint |
| member access | `obj.key` and `obj[int]` for objects, arrays, tuples and strings, including the negative array index and the native undefined results |
| tuple unpack errors | `Cannot unpack non-iterable type: <Type>`, `Too few items to unpack`, `Too many items to unpack`, `Cannot unpack non-identifier type: <Node>` |

Members were added in this slice because `loop.index` and the other fields are
unreachable without them. Filters, tests and function values are still native.

## Gate

`tests/stmt_fixture.cpp` parses a template with the native lexer and parser,
executes it with the native `jinja::runtime` as the oracle, encodes the same
context and AST for the port, and compares rendered output bytes or the
byte-exact exception text.

| corpus | cases |
|---|---|
| array iteration, all nine loop fields, neighbours, length, nested loops | 12 |
| `set` scope per iteration, break, continue, break-on-first-with-else, continue-only-with-else, `for`/`else` | 8 |
| object iteration (keys, key/value pairs) and tuple iterables | 6 |
| select filters, including loop fields over the filtered list | 3 |
| tuple unpack of array pairs, undefined iterable fallback | 2 |
| rejected templates (size mismatch on an array and on an object, non-iterable item, non-identifier target, invalid iterable) | 5 |
| total | **56 cases, 112 values, 0 failures** |
| mutations | **14/14** in two halves of seven |

Two mutations survived the first campaign and exposed corpus gaps rather than
port bugs: a missing `for`/`else` with a completed iteration (an "else always
runs" mutation passed) and a missing size-mismatch case over an object (the
"size is not checked" mutation passed). Both cases were added and both
mutations are caught.

The final `-O2` gate stays deferred for the same reason as EXP508: `gcc -O2` on
the generated module does not finish inside the five-minute per-step limit.
ASan is deferred as well.

## Build loop

`build_fixture.sh [mutate|final]`, measured here: goose 15.1 s, gcc 6.3 s
(`-O0`), fixture rebuilt 2.4 s, link 0.09 s, run 0.22 s, total 24.1 s.

## Scope

P7-08 stays PARTIAL: `for`, `break`/`continue`, the loop object and member
access are Goose. `macro`/`call`, filters and tests, function values, string
marking/caps and the chat layer remain native.
