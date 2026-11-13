# EXP516: ordering tests and `startswith`/`endswith` (P7-08 slice 5, part 6)

## Hypothesis

The ordering tests are `test_compare_fn<op>` over `value_compare`, and this
module already carries its exact port as `vcompare` (numeric pairs compare as
double, string/number and string/string pairs compare as text, bool pairs
support only `eq`/`ne`, every other pair is false). `startswith`/`endswith` are
a prefix/suffix byte compare behind `ensure_vals<string, string>`. If that holds,
neither a new value shape nor a new buffer is needed.

One point needs care: `vcompare` is dead code today — the binary operators use
`cmp_op`/`bytes_cmp` directly — and it reports the bool-type failure by setting
`st[1]` and the message at `E[0..st[2]]` rather than through `fail_node`. A test
node must wrap that message with the `Error executing TestExpression at position
N: ` prefix, so the call site has to build the failure itself.

## Native semantics (common/jinja/value.cpp)

| piece | rule |
|---|---|
| numeric pair | compared as double for `eq`/`ge`/`gt`/`lt`/`ne` |
| string pair or string/number | both sides go through `as_string` and compare as text |
| bool pair | only `eq`/`ne`; any ordering op raises `Unsupported comparison operator for bool type` |
| other pairs | `false` |
| names | `eq`, `equalto`, `ne`, `ge`, `gt`, `greaterthan`, `lt`, `lessthan`; there is no `le` entry in the native table |
| `startswith`/`endswith` | both operands must be strings; compare the prefix/suffix bytes |

## Plan

1. Add the ordering branch to `eval_test`, mapping the six names to the `vcompare`
   op codes (0 eq, 1 ge, 2 gt, 3 lt, 4 ne) and wrapping a `vcompare` failure with
   `fail_node(K_TEST, ...)`.
2. Add `startswith`/`endswith` to `eval_filter` as a byte prefix/suffix compare.
3. Extend the fixture: numeric, mixed string/number, string/string, bool error,
   `is not`, empty prefix and both pass/fail directions.
4. Gate, at least 10 mutations in two halves, docs, one commit.

## ACCEPT

- Gate green over the whole corpus.
- At least 10 mutations, all caught, baseline green after restore.
- The bool ordering error text matches the native wrapper exactly.
- No new fixed-size buffers.

## Result

Both parts are in place and the corpus passes:

```
FG516_RUNTIME_CMP cases=206 values=412 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=206 values=412 failures=0
```

Ordering tests `lt`/`lessthan`, `gt`/`greaterthan` and `ge` call `vcompare` with
the native op codes; its bool-type failure is wrapped with
`fail_node(K_TEST, ...)` so the template error reads
`Error executing TestExpression at position N: Unsupported comparison operator
for bool type`. `startswith`/`endswith` compare prefix/suffix bytes behind an
explicit both-sides-string check.

One case was added after the first mutation round: dropping `greaterthan` from
the gt mapping was invisible while every fixture case compared unequal values
(`2 >= 1` and `2 > 1` agree), so `{{ 2 is greaterthan(2) }}` was added to pin
strictness.

Mutations: lt as gt; gt as lt; ge as gt; `lessthan` unmapped; `greaterthan`
unmapped; bool ordering error swallowed; `startswith` takes the suffix path;
`endswith` takes the prefix path; prefix compare inverted; suffix index shifted;
empty pattern rejected; a match reported as false.

## Not ported

- `le`/`lessequal`/`lessequalto`: the native table has no `test_is_le`, so the
  name stays an unknown test.
- Wrong-type operands for `startswith`/`endswith`: the native `ensure_val`
  message embeds a compiler-specific `typeid(T).name()`, so the port returns a
  stable `filter expects String values` and the fixture does not exercise it.
- `startswith`/`endswith` for tuples/arrays, which the native string table does
  not provide.
