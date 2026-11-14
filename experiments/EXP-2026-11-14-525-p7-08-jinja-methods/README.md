# EXP525: bound method values (P7-08 slice 5, part 15)

## Hypothesis

The native member access prefers a type's built-in over an object key for a
static property (`obj.prop`), and returns a `value_func` bound to the object
(`arg0`). Our runtime resolves attributes to plain values, so a method call such
as `'ab'.upper()` needed a function value.

Rather than duplicating every filter body, this slice reuses `eval_filter`: a
method value carries the receiver and the name bytes, and a call builds four
spare AST nodes (a value literal, an identifier, a call node that reuses the
call site's argument list, and a filter node) before delegating. The fixture
reserves four extra AST slots for that purpose.

That worked, with two defects found by the gate: the name bytes were read from
the AST fields instead of the string value fields, and the method table must be
per type — an object has no `length` builtin, so `loop.length` must read the key.

## Native semantics (common/jinja/runtime.cpp)

| access | rule |
|---|---|
| static `obj.prop` | the type's built-in wins over an object key; if there is no built-in, the key is read |
| computed `obj["prop"]` | the key wins for an object; a missing key falls back to a built-in |
| array/string with a string property | the type's built-in |
| built-in tables | per type: strings have `upper`/`lower`/`strip`/`length`/... ; arrays have `first`/`last`/`length`/`sort`/... ; objects have `keys`/`values`/`items`/`get`/... and no `length` |
| method value | `value_func` bound to the receiver; `is callable` is true |

## Plan

1. Add `V_METHOD` (kind 11) carrying the receiver and the name bytes; resolve it
   in `eval_member` with the native priority order, restricted per type.
2. Add `K_VALUE` (kind 27) and `call_method`, which builds the four spare nodes
   and calls `eval_filter`.
3. Reserve four extra AST slots in the fixture.
4. Fixture cases: string methods with and without arguments, array `first`/
   `length`/`sort`, object `keys`/`get`, a static function reference that must
   not render, `is callable`, chained methods, `loop.length` (the object key) and
   an object whose key collides with a builtin name.
5. Gate, at least 12 mutations in two halves, docs, one commit.

## ACCEPT

- Gate green over the whole corpus.
- At least 12 mutations, all caught, baseline green after restore.
- The existing member access (arrays, objects, `loop.*`) is unchanged.
- No new fixed-size buffers in the runtime; the spare nodes are AST slots.

## Not ported

- Methods on scalars (int/float/bool have `int`/`float`/`string`/`tojson` in the
  native tables) and `slice`/`selectattr`/`rejectattr`/`append` as methods.
- Computed access fallback to a built-in for a missing object key is only
  reached through the same `method_value` call, which is limited to the tables
  above.
