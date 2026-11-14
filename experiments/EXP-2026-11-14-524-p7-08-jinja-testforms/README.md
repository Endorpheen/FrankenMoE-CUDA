# EXP524: select/reject test forms and map defaults (P7-08 slice 5, part 14)

## Hypothesis

The 3- and 4-argument forms of `select`/`reject` apply a test to the item or to
its attribute, and `map` accepts a `default` keyword for a missing attribute.
All three reuse machinery that already exists — the test dispatcher, `attr_of`
and the keyword capture — if the test dispatch is lifted out of the
`test_expression` node into a reusable function.

That held. `eval_test` now extracts the operand and test name and delegates to
`dispatch_test`, which takes the value, the argument slice, the position and a
mode flag; the node keeps only the negation.

## Native semantics (common/jinja/value.cpp, `selectattr`)

| form | rule |
|---|---|
| `arr \| select(attr)` | attribute truthiness (EXP520) |
| `arr \| select(test, value)` | the test is applied to the item itself |
| `arr \| select(attr, test, value)` | the attribute is resolved first, then the test is applied to it |
| unknown test | `selectattr: unknown test '<name>'`, raised from the filter, so it is wrapped as a FilterExpression |
| `map(attribute=..., default=...)` | a missing attribute resolves to the default instead of `undefined` |

The unknown-test message differs between the two entry points: a test node says
`Unknown test '<name>'` and is wrapped as a TestExpression, while the select
path says `selectattr: unknown test '<name>'` and is wrapped as a
FilterExpression. The dispatcher therefore carries a mode flag rather than one
shared message.

## Plan

1. Lift the dispatch body out of `eval_test` into `dispatch_test` with a position
   and a mode parameter; keep the negation in the node.
2. Add the 3- and 4-argument forms of `select`/`reject` and the second keyword
   slot for `map`'s `default`.
3. Fixture cases: select/reject with `gt`, select/reject with an attribute, a
   test and a value, map with a default on a mixed array, and an unknown test
   through the select path.
4. Gate, at least 12 mutations in two halves, docs, one commit.

## ACCEPT

- Gate green over the whole corpus.
- At least 12 mutations, all caught, baseline green after restore.
- Both unknown-test messages are exercised.
- The existing test-node behaviour is unchanged after the refactor.

## Not ported

- `strftime_now`: the native implementation formats `std::time(nullptr)` with
  `std::localtime` inside `strftime`. There is no deterministic time input in
  the native context (it reads the wall clock), and calling libc `strftime`
  would need a `struct tm` that this runtime does not have. A byte-identical
  port would either be time-dependent in the fixture or need a host clock and a
  C shim, so it is deliberately left native and recorded here.
- Bound method values (`value_func` with `arg0` from attribute access); the
  attribute path still returns plain values.
