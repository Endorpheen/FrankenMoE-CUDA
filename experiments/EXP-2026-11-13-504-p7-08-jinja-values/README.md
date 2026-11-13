# EXP504: Jinja value model in Goose (P7-08 third slice)

The value layer under the runtime: what a value is, how it converts, how it
compares, and how it prints. Built-ins and JSON output are the next slice; this
one owns the model they build on.

## What was ported

`source/values.goose`, from `value.h` and `value.cpp`:

| decision | rule |
|---|---|
| types | Integer, Float, String, Boolean, Array, Tuple, Object, None, Undefined (type text carries the hint) |
| flags | `is_numeric` (int/float/bool), `is_immutable` (array and object are mutable), `is_hashable` (undefined is not; containers recurse, and the object check looks only at the values) |
| as_int | int, float (its stored integer part), bool |
| as_float | int and float only; a bool throws `Boolean is not a float value`, as native does |
| as_string | decimal integers, the `std::to_string` double with trailing zeros stripped and one `.0` left, `True`/`False`, `None`, and container reprs (`[...]`, `(...)` with the single-element comma, `{k: v}`) |
| as_bool | zero/empty is false for every type; undefined and none are false |
| equality | `operator==` is equivalence: numeric across int/float/bool on both the integer and the float part, structural for strings, elementwise for hashable arrays/tuples/objects; `operator!=` is the strict per-type form |
| `value_compare` | numeric pairs compare as double, string pairs (and string with number) compare as text, bool pairs support only `==`/`!=` and throw `Unsupported comparison operator for bool type` for ordering, every other pair is false |
| repr | a string is `'...'`, or JSON-encoded when it contains a quote; other values use `as_repr` (the type text for undefined) |
| error texts | `<Type> is not an int/float/string value`, with the Undefined hint included |

Executable decisions stay out of scope: `get_builtins` (the per-type function and
filter tables), arithmetic operators, input marking and `value_to_json` belong to
EXP505 and the runtime/string slices.

## Gate

`tests/values_fixture.cpp` builds native values from a small DSL
(`i42`, `f1.5`, `s"text"`, `bT`, `n`, `u"hint"`, `a[...]`, `t[...]`,
`o{key=value,...}`), encodes the same value into the flat arrays, and compares:

- **info**: type, the three flags, none/undefined, `as_bool`, `as_int`,
  `as_float` (17 digits), `as_string` and `value_to_string_repr`, each with the
  exact exception text when the conversion is invalid;
- **compare**: the five `value_compare` operators and `operator==`/`operator!=`
  for every pair of a 15-value set.

| corpus | cases |
|---|---|
| values: integer boundaries, signed/negative zero and finite floats down to the min subnormal, strings with quotes/newlines/`True`, booleans, none, undefined with and without hint, arrays/tuples/objects including nesting and non-string keys | 46 |
| pairs: 15 x 15 | 225 |
| total | **271 cases, 271 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |
| mutations | **20/20** (truthiness per type, numeric flag, immutability, hashability of undefined/object, numeric and string equality, array length, strict float inequality, numeric/string compare, bool ordering, float as_string, repr quoting, integer sign, single tuple comma) |

Two mismatches were caught by the gate while porting: bool must not succeed in
`as_float` (native only overrides it for int and float), and undefined must not
be hashable (the default is false and undefined does not override it).

## Scope

`value_to_json` and `float_repr` were removed from this slice on purpose: no
value-model path calls them, so they were moved to EXP505 together with the
built-ins, where the gate can reach them.

P7-08 stays PARTIAL: built-ins/filters, runtime, string, caps, the chat layer
and the per-model parsers remain. Slice order:
`docs/goose-migration/P7-08-COVERAGE.md`.
