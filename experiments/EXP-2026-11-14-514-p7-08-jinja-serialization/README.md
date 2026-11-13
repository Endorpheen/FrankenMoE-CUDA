# EXP514: filters `tojson`, `string`, `safe`, `int`, `float` (P7-08 slice 5, part 4)

## Hypothesis

The serialization and conversion filters reuse machinery that already exists in
the Goose module: `json_into` (ported with the value model in EXP504/EXP505) for
`tojson`, `float_repr` for float text, `as_string_into` for coercion and the
integer/float value constructors for `int`/`float`. If that holds, no new value
shape, no new buffer and no new runtime service is needed, and the differential
fixture can prove the port.

A second question the fixture must answer: the native tables for `string` and
`safe` are per type and differ from each other (`string` on a string is an
identity, `safe` on a string serializes), so a single shared rule would be a
wrong port.

## Native semantics (common/jinja/value.cpp)

| filter | int | float | bool | string | none | undefined | array/object |
|---|---|---|---|---|---|---|---|
| `int` | identity | truncation | 1/0 | `stoi` with `default`/`base` | not present | not present | not present |
| `float` | to float | identity | 1.0/0.0 | `stod` with `default` | not present | not present | not present |
| `string` | `tojson` | `float_repr` | `"True"/"False"` | identity | `"None"` | `""` | `tojson` |
| `safe` | `tojson` | `float_repr` | `"True"/"False"` | `tojson` | `"None"` | `""` | not present |
| `tojson` | `tojson` | `tojson` | `tojson` | `tojson` | `tojson` | not present | `tojson` |

`tojson` is `value_to_json(value, indent, item_sep, key_sep)` with
`indent = -1`, `item_sep = ", "`, `key_sep = ": "` by default; positional
arguments are `ensure_ascii`, `indent`, `separators`, `sort_keys`. A truthy
`sort_keys` raises `tojson sort_keys=true not implemented`.

`int` on a string is `std::stoi(str, nullptr, base)`: leading whitespace and an
optional sign are accepted, parsing stops at the first invalid byte, and a string
with no digits falls back to `default` (or `0`). A base outside `0`/`2..36`
raises `int() base must be 0 or between 2 and 36`.

## Plan

1. Implement `tojson` on `json_into` for every type except `undefined`.
2. Implement `string`/`safe` with the per-type table above.
3. Implement `int`/`float` for numeric, bool and string operands with the
   default/base rules; non-decimal bases are out of scope for this slice.
4. Extend the fixture with cases that pin each cell of the table, then let the
   differential gate correct any wrong guess (`got`/`want` output).
5. Gate, at least 10 mutations in two halves, docs, one commit.

## ACCEPT

- Gate green over the whole corpus.
- At least 10 mutations, all caught, baseline green after restore.
- Per-type differences between `string` and `safe` are visible in the fixture.
- No new fixed-size buffers; the output scratch and cursor stay the only
  mechanism.

## Result

Both hypotheses held, with two corrections that the differential gate produced:

- `safe` on a string is an identity, not a serialization. The first
  implementation followed a wrong reading of the table and the gate rejected it
  with `got=["a"] want=[a]`.
- `string` on an array or object is the value repr (`{'k': 1, 'j': 2}`), not
  JSON. `repr_into` reproduces it; `json_into` was wrong there.

The table that ships:

| filter | behaviour |
|---|---|
| `tojson` | `json_into` with `indent = -1`, `", "` and `": "` separators; `undefined` is an unknown-filter error, everything else (including `none` as `null`) serializes |
| `string` | string identity; int/bool/float/none as their text; undefined as `""`; array/tuple/object as repr |
| `safe` | same as `string` except that arrays, tuples and objects are absent (unknown-filter error), as in the native tables |
| `int` | int identity; float truncation; bool 1/0; string prefix parse with sign and `default` |
| `float` | float identity; int/bool conversion; string prefix parse with sign, fraction, exponent and `default` |

Gate: **153 cases, 306 values, 0 failures** (the 122 of EXP513 plus 31).
Mutations: **12/12** in two halves, baseline green after each restore.

Mutations: tojson accepts undefined; tojson indents; `string` on a string stops
being an identity; bool False rendered as True; None lowercase; undefined as
`x`; array `string` as JSON; `int` truncates upward; `int` ignores the sign;
`int` ignores the default; `float` ignores the exponent; `float` ignores the
fraction.

## Not ported

- `tojson` options: `ensure_ascii`, `sort_keys` and the `separators` array. The
  no-argument form (the common template case) is complete.
- Non-decimal bases for `int`: a base other than `0`/`10` raises an explicit
  `int() non-decimal base not ported` instead of parsing.
- Integer overflow during the prefix parse is not detected; `std::stoi` would
  raise and fall back to `default`.
- Undefined-variable hints: the native context binds a missing name with
  `hint = <name>`, and that hint appears in error text. Our context binding does
  not attach hints yet, so the fixture passes an explicit hint for `u`. This is
  a pre-existing context gap, not part of this slice.
- The integer/float suffix rules of `std::stoi`/`std::stod` for `nan`, `inf` and
  trailing garbage beyond the prefix.
