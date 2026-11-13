# EXP515: Jinja tests `is ...` (P7-08 runtime slice 5, part 5)

## Hypothesis

A test is the same machinery as a filter under a different name: the native
`test_expression` executes the operand, then dispatches `test_is_<name>` through
the global built-in table, evaluates the call arguments and negates the result
for `is not`. If the flat AST carries the node, the Goose runtime can reuse
`eqv`, `test_in`, `vi`, `vk` and `vstr` and needs no new value shape; the fixture
encoder is the only missing piece.

That held. The fixture did not encode `test_expression` at all
(`grep test_expression tests/stmt_fixture.cpp` returned nothing), so the node
had to be added to both sides of the differential gate.

## Native semantics (common/jinja/runtime.cpp, value.cpp)

| piece | rule |
|---|---|
| node | `test_expression { operand, negate, test }`; `test` is an identifier or a call whose callee must be an identifier (`Test callee must be an identifier`) |
| dispatch | the name maps to `test_is_<name>` in `global_builtins`; an unknown name raises `Unknown test '<name>'`; `negate` inverts the result |
| type tests | `defined`, `undefined`, `none`, `boolean`, `string`, `integer`, `float`, `callable`, `mapping`, plus `number` (int/float), `iterable`/`sequence` (object, array, string, undefined) |
| identities | `true`/`false` require a bool and its value |
| numeric | `odd`/`even` require an integer; `divisibleby(n)` requires two integers and tests `a % n == 0` |
| text | `lower`/`upper` require a string and reject any byte of the opposite case |
| comparison | `eq`/`equalto`/`ne` compare with the value equivalence |
| membership | `in` uses the same `in` operator as expressions, including its string rule |

## Plan

1. Add `K_TEST = 26` to the runtime, name it `TestExpression`, dispatch to
   `eval_test`, and encode `operand`, `test` and the `negate` flag in the
   fixture.
2. Implement the table above on `eqv`, `test_in` and the byte accessors; build
   the unknown-test message through the output scratch and rewind the cursor.
3. Extend the fixture with two cases per test family, including `is not` and an
   unknown test name.
4. Gate, at least 10 mutations in two halves, docs, one commit.

## ACCEPT

- Gate green over the whole corpus.
- At least 10 mutations, all caught, baseline green after restore.
- The negate path and the unknown-test error are both exercised.
- No new fixed-size buffers.

## Result

The node and the dispatcher are in place and the whole corpus passes:

```
FG515_RUNTIME_TESTS cases=187 values=374 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=187 values=374 failures=0
```

Implemented tests: `defined`, `undefined`, `none`, `boolean`, `true`, `false`,
`string`, `integer`, `float`, `number`, `iterable`, `sequence`, `mapping`,
`callable`, `odd`, `even`, `divisibleby(n)`, `lower`, `upper`, `eq`, `equalto`,
`ne`, `in`, plus `is not` and the unknown-test error.

One case was added after the first mutation round: `none` accepted strings
because no fixture case probed `is none` on a non-none value
(`{{ 'a' is none }}`, `{{ 1 is none }}`). With those cases the mutation is
caught.

Mutations: defined inverted; undefined inverted; `none` accepts strings; number
excludes floats; odd/even swapped; `divisibleby` inverted; `lower` inspects the
wrong case; `eq` inverted; `in` ignores its result; negate ignored; iterable
excludes arrays/tuples; mapping accepts arrays.

## Not ported

- Ordering tests (`lt`, `le`, `gt`, `ge`, `lessthan`, `greaterthan`); only
  equality is covered here.
- `is sameas`, `is filter`, `is test`, `is escaped` and the remaining
  `global_builtins` entries.
- Wrong-type arguments for `odd`/`even`/`divisibleby`/`lower`/`upper`: the native
  `ensure_val` message embeds a compiler-specific `typeid(T).name()`, so the
  port returns a stable `test expects ...` message instead and the fixture does
  not exercise that path.
