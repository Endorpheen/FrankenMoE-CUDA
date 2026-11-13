# EXP476: integer bounds and const/enum rules in Goose (P7-09 fifth slice)

Integer schemas turn `minimum`/`maximum` into a compact GBNF expression over
decimal digit ranges; consts and enums turn JSON values into quoted literals.
This slice ports both and gates them against the real `build_grammar` oracle.

## What was ported

`source/bounds.goose`, from `build_min_max_int`, `digit_range`, `more_digits`,
`uniform_range` and `_generate_constant_rule`:

| decision | rule |
|---|---|
| digit range | `[c]` when the endpoints are equal, `[a-b]` otherwise |
| digit count | `[0-9]` alone for exactly one digit, else `[0-9]{min}` or `[0-9]{min,max}` with the upper bound omitted when unbounded |
| uniform range | keep the shared prefix as a quoted literal, then split on the first differing digit, with the zeros/nines sub-ranges and the `to_reached` optimisation |
| both bounds | negative/negative via `"-" (…)`, negative/positive split, the digit-length loop that widens the minimum (`1` + zeros), then one uniform range |
| only a minimum | negative, zero, single-digit and multi-digit branches, the last recursing on the decimal suffix with the 16-step budget |
| only a maximum | non-negative (`"-" [1-9] …` at top level, then a both-bounds call from zero) and negative (`"-" (…)` with an unbounded minimum) |
| no bounds | native throws `At least one of min_value or max_value must be set`; the port returns -1 |
| const | `format_literal(value.dump())` |
| enum | `"("` + values joined by `" | "` + `")"` |

The port reproduces a native edge case exactly: with `minimum = INT64_MIN + 1`
and no maximum, the negated minimum equals the `INT64_MAX` sentinel, so the
recursive call sees no bounds and native throws. The fixture asserts that the
Goose side signals `-1` for exactly those inputs.

## Gate

`tests/bounds_fixture.cpp` builds real `common_chat_schema_integer`,
`common_chat_schema_const` and `common_chat_schema_enum` values, runs the native
`build_grammar` and compares the root body.

| corpus | cases |
|---|---|
| integers, one bound: 29 values as a minimum and as a maximum | 58 |
| integers, both bounds: 15-value grid, `min <= max` | 120 |
| integer extremes (`-INT64_MAX..-1`, `INT64_MIN+1..INT64_MAX-1`) | 2 |
| consts: strings with quotes/backslashes/newlines/unicode, numbers, bools, null, arrays, objects | 15 |
| enums: empty, one, two, mixed types, escaped strings, nested arrays/objects | 8 |
| total | **203 cases, 203 values, 0 failures** |
| ASan + UBSan over module and fixture | 0 failures, rc 0 |

Mutations 17/17 caught: single-digit shortcut (109), digit range (131), shared
prefix (114), tail branch (2), digit-length loop (115), next minimum `2` (115),
negative endpoint swap (7), negative minimum sign (9), zero split (2), range start
(4), multi-digit recursion (runtime trap), negative maximum leading zero (19),
negative maximum from zero (9), no-bounds signal (1), const dash (1), enum comma
(6), equal-bound printing (80).

## Scope

P7-09 stays PARTIAL. Still native: the visit dispatch and recursion, `$ref`
targets, `_not_strings`, allOf (including the enum intersection), the primitive
rule texts and the PEG parser.
