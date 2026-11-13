# EXP517: `split`/`rsplit` and the `truncate` cell (P7-08 slice 5, part 7)

## Hypothesis

`split`/`rsplit` build an array of substrings with `arr_push` and `mk_str` over
the existing string storage; no new value shape is needed. `truncate` is a
different case: this native tree has it only in the `undefined` built-in table
(an empty string), and the string type has no `truncate` at all, so porting it
means porting one cell of the undefined table, not an algorithm.

Both held.

## Native semantics (common/jinja/value.cpp)

| piece | rule |
|---|---|
| `split(s, delim=" ", maxsplit=-1)` | scan left to right, push the text before each delimiter, then push the remainder; `maxsplit` limits the number of splits (0 pushes the whole input); the default delimiter is a single space, with the native `FIXME` that consecutive whitespace is not collapsed |
| `rsplit` | the same from the right (`rfind`), pushing the text after each delimiter, then the remainder, then reversing the result |
| empty separator | `empty separator` raised |
| non-string operand | the type's table has no `split`, so `apply_filter` raises the unknown-filter error; the `"split() first argument must be a string"` message is unreachable through the filter path |
| `truncate` | only in the `undefined` table: returns `""`; `'abc'|truncate` is an unknown filter |

## Plan

1. Add `split`/`rsplit` for string operands to `eval_filter`, leaving every
   other type to the existing unknown-filter fallback so the native error text
   matches.
2. Push the remainder unconditionally and reverse the array for `rsplit` by
   swapping pool entries.
3. Add `truncate` to the undefined-value table.
4. Fixture cases: both directions, explicit and default delimiter, `maxsplit`
   of 0/1/absent, empty input, empty tokens, no delimiter present, empty
   separator error, non-string and undefined operands, and the two `truncate`
   cells.
5. Gate, at least 12 mutations in two halves, docs, one commit.

## ACCEPT

- Gate green over the whole corpus.
- At least 12 mutations, all caught, baseline green after restore.
- The empty-separator error text matches the native wrapper.
- No new fixed-size buffers beyond the message scratch already used.

## Result

```
FG517_RUNTIME_SPLIT cases=225 values=450 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=225 values=450 failures=0
```

Two corrections came out of the gate, both in our own code:

- the delimiter is `argv[0]` and `maxsplit` is `argv[1]`: the native code indexes
  `args` including the input value, our call form does not. With the native
  indices the whole array collapsed to one element.
- `split(',', 1)` showed that the guard must be `argc > 0` for the delimiter and
  `argc > 1` for `maxsplit`, not `argc > 1`/`argc > 2`.

Both fixes are recorded here because they were real defects, not fixture noise.

Mutations: default delimiter becomes a comma; split ignores maxsplit; rsplit
ignores maxsplit; split drops the remainder; rsplit is not reversed; rsplit
stops at the first match; the empty-separator error text differs; rsplit drops
the head; rsplit keeps the delimiter in the token; truncate is not answered for
undefined; split never stops after maxsplit; split starts after the first token.

## Not ported

- `split`/`rsplit` on non-string operands: the native type tables have no entry,
  so those names stay unknown filters, which the fixture confirms for an integer
  and for undefined.
- `maxsplit` values below `-1` behave as unlimited, as in the native loop.
- `truncate` for strings: the native string table has no `truncate`, so
  `'abc'|truncate` remains an unknown filter; only the `undefined` cell returns
  `""` and that is what this slice ports.
- `format`, `indent` and the container filters are still native.
