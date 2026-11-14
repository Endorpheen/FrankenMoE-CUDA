# EXP521: reverse, list, min/max, append/pop and join(attribute=) (P7-08 slice 5, part 11)

## Hypothesis

The remaining container filters are loops over the child range plus `vcompare`
and `attr_of`: `reverse` copies in reverse order, `list` copies (keys for an
object), `min`/`max` fold with `vcompare`, `append`/`pop` build a new array, and
`join(attribute=)` resolves the attribute before the existing string
conversion. No new value shape is needed.

One native detail matters: `append`/`pop` mutate the array in place, while our
values are immutable inside an append-only pool, so the port returns a new
array. Outputs that only observe the filter result are identical; the
difference appears only when a template inspects the same array afterwards, and
that divergence is recorded below rather than hidden.

Native `sum` is not a real filter in this tree: only the undefined table answers
it (with `0`), so `array|sum` stays an unknown filter.

## Native semantics (common/jinja/value.cpp)

| filter | rule |
|---|---|
| `reverse` | array or tuple: copy and reverse; the result keeps the tuple kind |
| `list` | array/tuple → a copy as an array; object → an array of keys |
| `min` / `max` | array or tuple only; empty returns `undefined`; the `attribute` (keyword or second positional) is resolved per item and compared with `value_compare`; the result is the item, not the attribute |
| `append(v)` | array only; pushes `v` and returns the array |
| `pop(idx=-1)` | array only; a negative index counts from the end; returns the removed element |
| `join(delim, attribute)` | resolves the attribute per element before the string conversion |
| `sum` | undefined table only: `0`; no array entry |

## Plan

1. Implement `reverse`, `list`, `min`/`max`, `append`, `pop` and the
   `attribute` form of `join`; report `sum` for undefined as `0`.
2. Reuse `attr_of` for the attribute forms and `vcompare` for the folds.
3. Fixture cases: reverse on an array and a tuple, list on an object and an
   array, min/max on numbers, an empty array and an attribute, append and its
   length, pop at the tail, by index and with a negative index, join with an
   attribute, and the two `sum` cells.
4. Gate, at least 12 mutations in two halves, docs, one commit.

## ACCEPT

- Gate green over the whole corpus.
- At least 12 mutations, all caught, baseline green after restore.
- `reverse` keeps a tuple a tuple and `pop` resolves negatives.
- No new fixed-size buffers.

## Not ported

- In-place mutation of `append`/`pop`: our arrays are immutable pool values, so
  both return a new array (or the popped element). A template that inspects the
  original array after the filter would see the original value.
- `min`/`max`/`join` attribute forms on nested attributes.
- `sum` for arrays (native has no such filter in this tree).
