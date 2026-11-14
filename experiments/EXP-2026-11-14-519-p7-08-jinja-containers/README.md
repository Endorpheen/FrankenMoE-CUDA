# EXP519: object accessors and basic container filters (P7-08 slice 5, part 9)

## Hypothesis

`keys`, `values`, `items`, `get`, `sort` and `unique` need no new value shape:
objects already store parallel key/value arrays (`vks`/`vvs`/`vkc`), arrays are a
contiguous child range in the pool, and the comparison needs are `vcompare` and
`eqv`. A Goose port is therefore a set of loops over existing accessors, with
`items` building two-element `V_TUPLE` values and `sort` an insertion sort.

The risky part is not the data but the option surface: `sort` and `unique` accept
`attribute`, and the native `map` requires a keyword for its attribute. This
slice keeps only the argument forms that have no attribute resolution, and says
so explicitly.

## Native semantics (common/jinja/value.cpp)

| filter | rule |
|---|---|
| `keys` | object only: array of keys in insertion order |
| `values` | object only: array of values in insertion order |
| `items` | object only: array of `(key, value)` tuples in insertion order |
| `get(key, default=none)` | object only; the key must be a string; a missing key returns the default (an explicit third argument, otherwise `none`) |
| `sort(reverse=false, case_sensitive=false, attribute)` | array or tuple: `value_compare` with `lt` (or `gt` when `reverse`); the native code ignores `case_sensitive` and always sorts case-sensitively; `attribute` is not part of this slice |
| `unique(case_sensitive=false, attribute)` | array or tuple: keep the first item whose comparison key has not been seen; `case_sensitive` is ignored natively |
| `dictsort(...)` | sorts the object's internal pair vector and returns the object; excluded here because our object layout would need a parallel reorder of the key and value arrays |

## Plan

1. Add `keys`, `values`, `items`, `get` for object operands; non-objects fall
   through to the unknown-filter error like the native tables.
2. Add `sort` and `unique` for arrays and tuples without an attribute argument:
   insertion sort over a copied child list, comparison through `vcompare`.
3. Fixture cases: insertion order for four-key objects, `get` hit/miss with and
   without a default, `sort` on ints and strings, `reverse`, `unique` on
   duplicates, tuple results, and wrong-type operands.
4. Gate, at least 12 mutations in two halves, docs, one commit.

## ACCEPT

- Gate green over the whole corpus.
- At least 12 mutations, all caught, baseline green after restore.
- Insertion order of objects is preserved by `keys`/`values`/`items`.
- No new fixed-size buffers.

## Result

```
FG519_RUNTIME_CONT cases=257 values=514 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=257 values=514 failures=0
```

`items` needed one layout decision. The pool holds a value's children as one
contiguous range, so pushing a tuple's two children between two pushes of the
outer array would break the outer range. The implementation therefore reserves
all outer slots first, then builds each tuple after the reserved block and
patches the slot, which keeps both ranges contiguous.

`sort` is an insertion sort over the result's own pool range with `vcompare`
op 3 (ascending) or op 2 (`reverse`), so equal elements keep their order.
`unique` keeps the first item whose comparison against every already kept item
is false.

Mutations: keys/values swapped both ways; items repeats the key; items tuples
hold one element; get ignores the default; get ignores the hit; sort descending
by default; sort ignores reverse; sort places the key at the wrong slot; unique
keeps duplicates; unique drops the first occurrence; items loses the tuple
reference.

## Not ported

- `attribute` for `sort`/`unique`, `map` (native requires a keyword argument and
  rejects filter mapping), `select`/`reject`/`selectattr`/`rejectattr`.
- `dictsort`: the native implementation sorts the object's internal pair vector
  in place; our object stores parallel key and value arrays, so it would need a
  coordinated reorder and is left for a later slice.
- `append`, `pop`, `reverse`, `min`, `max`, `sum`, `list` and `join` with
  `attribute`.
