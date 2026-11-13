# EXP493: JSON text parser in Goose (P7-09 twenty-second slice)

The JSON decoder behind `common_json::parse`: the RFC 8259 grammar with
nlohmann's number classification, string escape rules, duplicate-key policy and
error classes. The JSON container type and `dump` stay native in this slice.

## What was ported

`source/jsonparse.goose`:

| decision | rule |
|---|---|
| grammar | objects, arrays, strings, numbers, `true`/`false`/`null`, whitespace ` \t\n\r` |
| integers | non-negative up to `UINT64_MAX` stay integers (signed when <= `INT64_MAX`, unsigned above); `-0` is `0`; `-2^63` is the minimum; anything wider falls back to float |
| floats | any token with `.` or an exponent, or an integer that overflowed, is parsed by `strtod`; an infinite or NaN result is `out_of_range.406` |
| strings | escapes `" \ / b f n r t uXXXX`; a high surrogate must be followed by a low one; a lone low surrogate is rejected; raw bytes must be valid UTF-8 |
| duplicate keys | the value is replaced in place, the first position is kept |
| errors | parse_error.101 and out_of_range.406, with the library's line and column |
| position | one count per consumed byte, reset on `\n`; an end-of-input error is one past the last byte; a separator error is reported after the next token is read |

The single primitive borrowed from C is `strtod` (`gs_json_strtod`, declared as
an extern and supplied by the harness), because the shortest/correctly-rounded
decimal conversion is libc's job; every decision around it is Goose.

## Gate

`tests/json_fixture.cpp` parses the same text with the native `common_json::parse`
and with `json_parse`, then compares the tree node by node (kind, integer value,
double bits, string bytes, child order, object keys), and for rejecting inputs
the error class and the line/column:

| corpus | cases |
|---|---|
| accepted: literals, integer boundaries (`INT64_MIN/MAX`, `UINT64_MAX`, one past), floats and exponents, underflow, all escapes, surrogate pairs, nesting, duplicate keys, key order, whitespace | 40 |
| rejected: truncated/missing tokens, trailing commas, bad separators, leading zero, bare `-`, broken numbers, unknown escapes, short `\u`, lone/mismatched surrogates, raw control byte, invalid UTF-8, trailing garbage, vertical tab, number overflow | 34 |
| deep: 500 nested arrays | 1 |
| large: 20 000-element array, 5 000-key object, 40 000-byte string | 3 |
| total | **78 cases, 81 850 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |

Mutations 21/21 caught: leading zero, exponent digits, fraction digits, control
bytes, unknown escape, both surrogate directions, pair arithmetic, ill-formed
UTF-8, duplicate keys, uint boundary, unsigned boundary, sign, trailing garbage,
literal validation, overflow, boolean value, whitespace set, error column, list
flip, separator position.

Two mutations were dropped as equivalent: routing `+` into the number parser (it
is rejected at the same byte either way) and removing the `-2^63` special case
(the wrap-around lands on the same value).

## Scope

P7-09 stays PARTIAL: JSON `dump` (rendering), the PEG schema parsers built on
this tree and the grammar/PEG malformed corpora still need their own slices.
