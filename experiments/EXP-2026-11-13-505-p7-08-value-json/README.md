# EXP505: JSON value output in Goose (P7-08 fourth slice)

`value_to_json` with its two float formatters, the last self-contained part of
`value.cpp` before the built-in tables.

## What was ported

`source/valuesjson.goose`, from `value.cpp` (on top of EXP504's value model,
which the module carries so the slice is self-contained):

| function | rule |
|---|---|
| `float_repr` | Python's repr: the shortest digits that read back (start at 15 digits, 0 for subnormals; powers of two may need the next digit up), trailing zeros stripped, fixed for `1e-4 <= |x| < 1e16` and scientific with a two-digit exponent otherwise |
| `float_json` | `NaN`, `Infinity`, `-Infinity` for the non-finite cases, else `float_repr` |
| `value_to_json` | none/undefined -> `null`, booleans, integers, floats, strings with the escape table, arrays and tuples as arrays, objects in insertion order |
| layout | `indent < 0`: no newlines; `indent >= 0`: newline per item, `", "` separator before it, `": "` after a key; `indent > 0` also indents by level; empty containers inline |
| object keys | a float key goes through `float_json`, every other key through `as_string`, and the result is then JSON-escaped as a string |

## Gate

`tests/json_fixture.cpp` builds native values from the DSL (now with real escape
processing in strings), encodes them flat and compares `value_to_json` byte for
byte at indent `-1`, `0` and `2`:

| corpus | cases |
|---|---|
| scalars: integer boundaries, floats from `-0.0` and the min subnormal through `1e16`/`1e18` and 17-digit values to `Infinity`/`-Infinity`/`NaN`, strings with quotes, backslash, tab, newline, CR and control bytes `0x01`/`0x0b`/`0x1f`, booleans, none, undefined | 44 x 3 |
| containers: empty/singleton/nested arrays and tuples, objects with string, integer, boolean, `0.1` and `1e16` float keys, nesting | 17 x 3 |
| generated: 500 integers, 200 keys, a 5 000-byte string, 50 nested arrays, 30 nested objects | 5 x 3 |
| total | **192 cases, 384 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |
| mutations | **20/20** (fixed threshold, exponent padding/sign, read-back check, leading zeros, `.0`, scientific mantissa, `NaN`, infinity sign, quote and control escaping, hex case, array newline, item separator, key separator, float keys, tuples, none, closing indent, item indent) |

The gate found a real bug carried over from EXP504: the shared JSON string
writer emitted raw control bytes instead of `\n`, `\t`, `\b`, `\f`. EXP504's
repr only reached that path for strings containing a quote, and its corpus never
put a real control byte there, so it stayed invisible; this slice fixes it and
covers all five escapes plus `\u00xx` case.

## Scope

P7-08 stays PARTIAL: the built-in function and filter tables (`get_builtins`),
arithmetic operators, the runtime, string marking, caps, the chat layer and the
per-model parsers remain. Slice order:
`docs/goose-migration/P7-08-COVERAGE.md`.
