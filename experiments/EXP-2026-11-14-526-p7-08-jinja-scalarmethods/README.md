# EXP526: scalar methods and the abs filter (P7-08 slice 5, part 16)

## Hypothesis

The bound method machinery from EXP525 is per-type, so extending it to int,
float and bool is a table change plus one missing filter (`abs`). If that holds,
scalars gain `int()`, `float()`, `string()`, `safe()`, `tojson()` and (for
numbers) `abs()`, and a missing method on a scalar reports the native hint.

That held. The gate also exposed the native hint rule for scalar receivers: a
missing property reports the property name as the undefined hint, not
`object_property`.

## Why not string marking in this slice

`string_part::mark_input`/`all_parts_are_input` were read before choosing the
slice. Inside the jinja runtime the flag only feeds `format`'s `fmt_is_input`
and `from_json`'s marking; it never changes rendered output. The consumers that
observe it (`caps`, the chat layer) live outside this module, so a marking slice
could not be verified by the differential fixture and was left for the layer
that can observe it.

## Native semantics (common/jinja/value.cpp)

| type | methods |
|---|---|
| int, float | `abs`, `int`, `float`, `safe`, `string`, `tojson` |
| bool | `int`, `float`, `safe`, `string`, `tojson` (no `abs`) |
| `abs` | numbers only: int keeps its type, float keeps its type; other types have no entry, so the filter is an unknown-filter error |

## Plan

1. Add the `abs` filter for int and float.
2. Extend `method_name_is` with the scalar tables.
3. Report a missing scalar property with the property name as the hint.
4. Fixture cases: `abs` on positive/negative int and float, `int`/`float`/`
   string`/`tojson` on numbers, the bool methods, `abs` on a bool as an error,
   the integer filter form, and a missing bool method (hint check).
5. Gate, at least 12 mutations in two halves, docs, one commit.

## ACCEPT

- Gate green over the whole corpus.
- At least 12 mutations, all caught, baseline green after restore.
- `abs` keeps the operand type.
- No new fixed-size buffers.

## Not ported

- String marking and caps: not observable in templates, deferred to the chat
  layer slice.
- `default` as a method (all tables have it) and methods on undefined.
