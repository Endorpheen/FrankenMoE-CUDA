# EXP523: dict and namespace as function values (P7-08 slice 5, part 13)

## Hypothesis

`dict(...)` and `namespace(...)` are the native `toobject`: an object built from
keyword arguments, from one iterable of two-element pairs, or as a copy of an
object. With the builtin-value marker from EXP522 and a keyword-aware argument
pass, both can be implemented over `obj_key`/`obj_val` storage without new value
shapes.

That held, with one prerequisite confirmed first: `call_builtin` evaluated every
argument with `eval_node`, and `eval_node` has no `K_KWARG` branch, so
`namespace(a=1)` would have failed with "Unexpected node type". The builtin path
therefore handles `K_KWARG` nodes itself.

## Native semantics (common/jinja/value.cpp, `toobject`)

| piece | rule |
|---|---|
| keyword argument | inserts `key -> value`, key taken from the argument name |
| first positional argument | an object → all its pairs are copied in order; an array/tuple → each item must be a two-element array/tuple and becomes a pair |
| other positional argument | raises `namespace/dict() arguments must be kwargs, dict and/or iterable of tuples, not <Type>` |
| bad iterable item | raises `namespace/dict() iterable argument must consist of tuples, not <Type>` |

Our object layout keeps all keys in one contiguous pool range and all values in
another, so pairs are collected first (up to 8, a documented cap) and written to
the two reserved regions; the object cannot be filled pair by pair.

## Plan

1. Add `dict`/`namespace` identifiers resolving to builtin values (ids 3 and 4).
2. Implement the keyword-aware collection and the object copy in `call_builtin`,
   including the native error texts.
3. Fixture cases: kwargs, one keyword, object copy with order, tuple pairs,
   `namespace` stored in a variable, `callable`, empty object, kwargs after an
   iterable, a three-element pair error, a non-iterable error and a second
   positional error.
4. Gate, at least 12 mutations in two halves, docs, one commit.

## ACCEPT

- Gate green over the whole corpus.
- At least 12 mutations, all caught, baseline green after restore.
- Object copy preserves key order in both `keys` and `tojson`.
- No new fixed-size buffers beyond the small local pair arrays.

## Not ported

- More than 8 entries in one `dict`/`namespace` call (explicit error).
- Non-string keys through the iterable form are accepted as values; the native
  `insert` additionally enforces hashable keys.
- `strftime_now` and the remaining `global_builtins`.
