# EXP494: JSON text rendering in Goose (P7-09 twenty-third slice)

`common_json::dump` and `dump_safe` — the serializer behind JSON output for
chat responses, tool calls and debugging.

## What was ported

`source/jsondump.goose`, from nlohmann's serializer:

| decision | rule |
|---|---|
| compact | `indent < 0`: no whitespace, `{"a":1}`, `[1,2]` |
| pretty | `indent >= 0`: one newline per element, `indent` spaces per level, `": "` after a key, empty containers inline |
| string escapes | `\"` `\\` `\b` `\t` `\n` `\f` `\r`, other bytes below 0x20 as `\u00xx` (lowercase), every other byte verbatim |
| integers | decimal; values above `INT64_MAX` print as unsigned |
| doubles | shortest decimal that reads back to the same value, laid out like the library: fixed when `k <= n <= 15` (then `.0` appended), fixed when `0 < n <= 15`, `0.000digits` when `-4 < n <= 0`, otherwise `d[.igits]e±XX` with a two-digit exponent |
| `-0.0` | keeps its sign |
| dump_safe | ill-formed UTF-8 runs become U+FFFD instead of throwing |

The decimal digits of a double come from the C library's correctly rounded
`%.*e` through one extern (`gs_json_fmt_e`), the same boundary role `strtod`
plays in the parser. The shortest-precision search, the digit/exponent split and
the layout are Goose.

## Gate

`tests/json_dump_fixture.cpp` encodes the native tree flat, renders it with
`json_dump`, and compares byte for byte with `dump(indent)` for indent `-1`, `0`,
`2` and `4`:

| corpus | cases |
|---|---|
| values: null/bool/int boundaries/uint64 max, floats (1.0, -0.0, 0.1, 1e10, 1e15, 1e16, 1e-4, 1e-5, 1.5e-10, min subnormal, max double, pi, big float), strings (escapes, controls, DEL, unicode, emoji), containers (empty, nested, key order) | 38 |
| deep 500 arrays, 20 000-element array, 5 000-key object, long escaped string | 4 |
| dump_safe on ill-formed UTF-8 (lone 0xff, truncated 2- and 3-byte, a run inside text, an invalid key) | 5 |
| total | **188 cases (47 trees x 4 indents), 188 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |

Mutations 23/23 caught: quote/backslash/backspace/control escaping, hex case,
colon spacing in both modes, indent level, empty array, trailing commas in both
containers, key order, integer sign, unsigned printing, the fixed-notation
threshold, the `.0` suffix, the shortest-digit start, negative zero, exponent
padding and sign, the leading-zero branch, the U+FFFD width, and safe-mode
replacement.

## Scope

P7-09 stays PARTIAL until a closure audit: the PEG schema helpers that build
documents from this tree, the AST `find_by_*`/`visit` helpers and the grammar/PEG
malformed corpora still need their own slice or an explicit boundary.
