# EXP483: remaining non-AST PEG variants in Goose (P7-09 twelfth slice)

Lookahead, atomic, character classes, strings, until and the passthroughs
(`ac`, `gbnf`), completing the matching engine apart from rule/tag/schema AST
production.

## What was ported

`source/pegparse2.goose`, from `parser_executor` (plus the first slice's
literal/sequence/choice/repetition/any/space, kept for tree composition):

| variant | rule |
|---|---|
| `and` | child result's type at the start position; no input consumed |
| `not` | success -> FAIL(start); NEED_MORE -> NEED_MORE(start); failure -> SUCCESS(start) |
| `atomic` | the child's result unchanged (node clearing is AST-only) |
| `chars` | codepoint walk over the encoded ranges with negation; INCOMPLETE/INVALID handling with the "enough matches" shortcut; max bound; minimum check with the lenient exhausted-input NEED_MORE |
| `string` | scan to the delimiter; escapes `\<delim> \\ / b f n r t \uXXXX`; unknown escape and bad hex fail; EOF -> FAIL(start,pos) or NEED_MORE(start,pos) when lenient |
| `until` | longest scan to the first delimiter start (complete match or a prefix reaching the end of input); invalid UTF-8 is consumed; lenient end-of-input -> NEED_MORE |
| `ac`, `gbnf` | the child's result |

The chars ranges are parsed by the native builder and passed in as data, like
the primitive rule contents in EXP479.

## Gate

`tests/peg_parse2_fixture.cpp` builds 15 trees with the real builder (reading the
chars ranges and pushing extra until delimiters into the built arena), encodes
them for Goose, and compares `common_peg_arena::parse` with `peg_parse2` for
every (tree, input, start, flags).

| corpus | coverage |
|---|---|
| trees | and, not (short/long literal), atomic, gbnf, ac, chars (unbounded, bounded, star, digits, negated, min>max), string, until (one and two delimiters), chars+string |
| inputs | ASCII, `aaaa`, quoted strings with escapes and broken unicode escapes, unterminated strings, END/partial END, UTF-8, invalid bytes |
| starts | 0..2 |
| flags | none and lenient |
| total | **2 080 cases, 2 080 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |

Mutations 16/16 caught: And end, Not NEED_MORE, Not inversion, Atomic
passthrough, chars negation, chars max, chars incomplete/invalid shortcuts,
chars lenient minimum, string delimiter, unknown escape, unicode hex width,
string EOF, until partial prefix, until end, until lenient end.

## Scope

P7-09 stays PARTIAL. Still native: rule/tag/schema matching with AST nodes,
invalid-UTF-8 tracking on results, `build_grammar`, the schema parsers and the
JSON side.
