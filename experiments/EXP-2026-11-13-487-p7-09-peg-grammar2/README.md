# EXP487: PEG -> GBNF build_grammar, chars/string/schema (P7-09 sixteenth slice)

The remaining self-contained conversion branches: character-class patterns,
string contents and delegating schema parsers.

## What was ported

`source/peggram2.goose` (extends EXP486):

| variant | text |
|---|---|
| chars | the raw pattern plus the same repetition suffix table (`? * + {n,} {n} {n,m}`) |
| string | `( [^<d>\\] \| "\\" ( [<d>\\/ bfnrt] \| "u" [0-9a-fA-F]{4} ) )*` |
| schema | a delegating schema (`raw` and a string-capable node) emits its child; otherwise `add_schema` (out of scope) |

## Gate

The full rule map is compared against the native `build_grammar` on 29 trees:
the EXP486 base corpus plus chars (star/bounded/negated), string with `"` and
`'`, string+chars, a delegating schema over a literal and schema+literal.

| result | |
|---|---|
| cases / values | **29 / 29, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |
| mutations | **9/9** (chars suffix/pattern/comma, string classes/backslash/slash/hex width/delimiter/group opener, schema delegation) |

## Scope

P7-09 stays PARTIAL: `until`/`ac` grammar generation (`gbnf_excluding/including_grammar`,
the Aho-Corasick DFA), non-delegating schemas (`add_schema`), lazy triggers, the
schema parsers and the JSON side remain.
