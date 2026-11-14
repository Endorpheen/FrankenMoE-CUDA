# EXP520: map, select/reject and dictsort (P7-08 slice 5, part 10)

## Hypothesis

`map(attribute=...)`, `select`/`reject` and `dictsort` need no new value shape:
objects already expose parallel key and value arrays, arrays and tuples expose a
contiguous child range, and attribute resolution is a lookup plus an index.
`dictsort` is the only one that must build a new object, because the result has
to be ordered while our objects keep insertion order.

That held. Two supporting pieces were needed: keyword arguments had to be
captured in the filter call form (the argument loop previously assumed every
argument was positional), and a shared `attr_of` resolver was added for
object keys, array indices and all-digit string indices.

## Native semantics (common/jinja/value.cpp)

| piece | rule |
|---|---|
| `map(attribute=..., default=...)` | the first argument must be a keyword (`map: filter-mapping not implemented` otherwise); each item's attribute is resolved through the same `get_attribute` used by `select`, and a tuple stays a tuple |
| `select(x)` / `reject(x)` | array/tuple only: resolve `x` as an attribute, keep items whose attribute is truthy (select) or falsy (reject); the result is always an array |
| `get_attribute` | object + string key → the value or the default; array/tuple + integer index, or an all-digit string parsed as an index, → the element or the default; anything else → the default |
| `dictsort(case_sensitive, by, reverse)` | object only: sort the pairs by key, or by value when `by == "value"`, descending when `reverse` is truthy; `case_sensitive` is ignored natively; returns an object |

## Plan

1. Capture keyword arguments in the filter call form (`K_KWARG` nodes) so
   `attribute=` reaches the filter.
2. Add `attr_of` for object keys, integer indices and all-digit string indices.
3. Implement `map`, `select`/`reject` and `dictsort`; `dictsort` copies the key
   and value ranges into a new object and insertion-sorts both in step.
4. Fixture cases: map over an array and over a tuple, map by index, select and
   reject with a present and a missing attribute, dictsort by key, by value and
   reversed, plus the JSON view of the sorted object.
5. Gate, at least 12 mutations in two halves, docs, one commit.

## ACCEPT

- Gate green over the whole corpus.
- At least 12 mutations, all caught, baseline green after restore.
- `dictsort` keeps keys and values aligned under every order.
- No new fixed-size buffers.

## Result

```
FG520_RUNTIME_SEL cases=269 values=538 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=269 values=538 failures=0
```

The first mutation round caught only 2 of 6 and the second 5 of 6, each time
because the fixture lacked a case that separates the two behaviours:

- map over a tuple (array and tuple JSON look identical, so the case uses
  `|string`),
- select versus reject on the same array (both `|length` values were 1, the case
  now looks at `|first`),
- an object that misses the requested attribute,
- an attribute index at the last array position,
- a missing check that `dictsort` by value reads values: with ascending values
  the key comparison coincided, so an object with descending values was added
  (`{"a": 2, "b": 1}` sorted by value must give `b`, `a`).

Mutations: map keeps the item; map never returns a tuple; reject behaves like
select; select keeps everything; a missing attribute keeps the item; the index
bound is one short; dictsort always sorts by value; dictsort ignores reverse;
dictsort shifts values out of step; dictsort by value reads the key; dictsort
drops one pair from the count; dictsort compares with the wrong order.

## Not ported

- `select`/`reject` with a test name and value (3-arg) or an attribute plus test
  (4-arg); only the attribute-truthiness form is ported.
- `map` with `default=` or the filter-mapping form (native rejects the latter
  with `map: filter-mapping not implemented`).
- `selectattr`/`rejectattr` as separate names, the remaining container filters
  (`append`, `pop`, `reverse`, `min`, `max`, `sum`, `list`) and `join` with
  `attribute`.
