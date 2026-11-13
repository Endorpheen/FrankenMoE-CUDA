# EXP490: non-delegating schemas in PEG build_grammar (P7-09 nineteenth slice)

A schema parser that is not delegating calls the grammar builder's `add_schema`
and uses the returned rule name. This slice ports that decision on top of
EXP487's build_grammar.

## What was ported

`source/peggram3.goose`:

| decision | rule |
|---|---|
| delegating schema | raw + string-capable node -> the child's text (EXP487) |
| non-delegating schema | emit `add_schema`'s rule: name + marker text, and use the name as the parser's text |

The schema *content* is the JSON-schema->GBNF converter (already Goose in
EXP472-479); the fixture captures `add_schema` with a marker so the gate checks
the decision and the naming, not the converter output.

## Gate

The full rule map is compared with native `build_grammar`: the EXP487 corpus
(29 cases) plus a non-delegating schema alone and in a sequence (2 cases):
**31 cases, 31 values, 0 failures**; ASan+UBSan clean.

Mutations 5/5: add_schema skipped, empty entry text, empty returned name,
delegating schemas calling add_schema, wrong entry name.

The initial draft failed the gate (missing `s` entry, empty root) because the
fixture did not carry the schema name into the encoded node; fixed by setting
the node string from `p.name`.

## Scope

P7-09 stays PARTIAL: lazy triggers, the schema parsers and the JSON side remain.
