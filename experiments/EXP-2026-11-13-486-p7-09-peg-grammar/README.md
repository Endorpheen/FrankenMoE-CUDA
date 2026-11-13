# EXP486: PEG -> GBNF build_grammar, first slice (P7-09 fifteenth slice)

The PEG arena converts back to GBNF for grammar-constrained decoding. This slice
ports the conversion for literal/sequence/choice/repetition/any/space/rule/ref,
with tag/atomic unwrapping and reachable-rule collection.

## What was ported

`source/peggram.goose`, from `build_grammar`, `to_gbnf`, `effective_parser` and
`collect_reachable_rules`:

| decision | rule |
|---|---|
| literal | `gbnf_format_literal` (escape CR/LF/quote/backslash, wrap in quotes) |
| sequence | children joined by spaces, empty children skipped, sequence/choice children parenthesised |
| choice | children joined by ` \| `, nested choice children parenthesised (defensive: the builder flattens) |
| repetition | child (parenthesised when effective sequence/choice) plus `?`, `*`, `+`, `{n,}`, `{n}`, `{n,m}` |
| epsilon/start/end, and/not | empty text |
| any/space | `.` / `space` |
| rule/ref | the rule name |
| tag/atomic | unwrapped to the child |
| reachability | walk from the root, marking rules through rule/ref nodes (refs resolve through the rule table) |
| emission | each reachable rule (child text under the rule name), then `root` |

chars/string/until/schema/ac and lazy triggers are outside this slice.

## Gate

`tests/peg_gram_fixture.cpp` builds trees with the real builder, captures the
native `build_grammar` rule map, encodes the built arena, and compares the Goose
map.

| corpus | cases |
|---|---|
| sequence, literal escapes, choice, `a* a+ a? a{2} a{2,4} a{2,}`, seq with a choice child, choice with a sequence child, tag+atomic, space+any, epsilon in a sequence, rule root, two chained rules, repetition of a choice, choice with an epsilon branch, choice of a choice (flattened), seq tag(choice), rep atomic(choice) | 21 |
| total | **21 cases, 21 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |

Mutations 14/14 caught: literal escaping, sequence separator, sequence
parenthesisation, choice separator, optional suffix, repetition
parenthesisation, any/space spelling, rule vs child text, reachability marking,
root rule text, empty variants, tag/atomic unwrapping, `{n,m}` comma.

One defensive branch (parenthesising a nested choice) is unreachable through the
public builder because `choice()` and `operator|=` flatten nested choices; the
corresponding mutation is therefore equivalent for this corpus.

## Scope

P7-09 stays PARTIAL: chars/string/until/schema/ac grammar generation,
`gbnf_excluding/including_grammar`, lazy triggers, the schema parsers and the
JSON side remain.
