# EXP500: PEG builder decisions in Goose (P7-09 29th slice)

Most `common_peg_parser_builder` methods only store their arguments. Three carry
a decision: the rule-name cleaner, the sequence/choice splice, and the two
`rule()` overloads' registration policy.

## What was ported

`source/builder.goose`, from `rule_name`, `sequence`/`choice` and `rule`:

| decision | rule |
|---|---|
| rule name | every maximal run of bytes outside `[a-zA-Z0-9-]` becomes one `-` |
| flattening | `sequence`/`choice` splice a nested parser of the same kind into the parent's child list, in order; every other parser is kept |
| `rule(name, parser)` | one rule parser and one ref; the clean name registers the rule |
| `rule(name, fn)` | a ref when the clean name is registered already; otherwise a placeholder rule (`any`), then the real rule parser over the built parser, then a ref — the registry ends on the second parser |

## Gate

`tests/builder_fixture.cpp` uses the real builder and observes the arena:

- **rule names**: 13 raw names (spaces, dots, slashes, tabs, a non-ASCII name,
  dash-only names) through `rule(raw, literal)`; the clean name is read from the
  rule parser and compared with the port, and the registry must resolve that name
  to a rule parser carrying it.
- **flattening**: `sequence({sequence(A,B), C})` and
  `choice({choice(A,B), C})` — the outer parser's children are compared with the
  port's splice of the same two inputs.
- **registration**: `rule(name, parser)` and `rule(name, fn)` for clean and
  unclean names; the number of rule parsers with that name, whether one was
  created, the registry target (the last rule parser) and the returned parser
  kind (a ref) are compared. A pre-registered name through the builder overload
  must add no parser.

| | |
|---|---|
| cases / values | **20 / 64, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |
| mutations | **10/10** (runs not collapsed, dot accepted, underscore separator, no splice, reversed splice, first child only, two parsers for the plain overload, no placeholder, parser for an existing rule, no ref returned) |

## Scope

The remaining builder methods are constructors: they store their arguments and
the `json_*`/`python_*`/`marker`/`number`/`boolean`/`null` helpers only compose
primitives. They are documented as a boundary and exercised by the PEG gates of
481-491, which build every tree through them.

P7-09 stays PARTIAL on the last item: the malformed/deep/large grammar and PEG
corpus.
