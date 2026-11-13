# EXP496: converter output in Goose — format_grammar and check_errors (P7-09 25th slice)

The converter's last step: render the rule map, and turn recorded errors and
warnings into the messages the public entry point surfaces.

## What was ported

`source/schemafull.goose` (EXP492's builder plus):

| function | rule |
|---|---|
| `format_grammar` | every rule as `name ::= text\n`, entries ordered by byte-wise key comparison (the `std::map` order); a key that is a prefix of another sorts first |
| `check_errors` (errors) | `JSON schema conversion failed:\n` + the messages joined with `\n` |
| `check_errors` (warnings) | `WARNING: JSON schema conversion was incomplete: ` + the messages joined with `; ` + `\n` |
| `json_schema_to_grammar` wrapper | a schema-build failure is reported with that same `JSON schema conversion failed:` prefix |

## Gate

`tests/grammar_fixture.cpp`:

- **errors, natively**: for ten rejecting schemas (unknown type, empty enum,
  unresolved ref, and nested failures inside properties/anyOf/allOf/type
  array/tuple, plus wrong property/`minimum` types) it calls the real
  `json_schema_to_grammar`, takes the exception text, and compares it byte for
  byte with the ported builder's message wrapped by `conversion_failed`. This
  gates the whole error path end to end against native.
- **format_grammar, transcribed**: eleven synthetic maps (empty, single,
  unsorted pairs and triples, prefix keys, numeric suffixes, byte order across
  upper/lower/underscore/digits, a non-ASCII key, empty text, text containing
  `::=`, 200 rules) are rendered by the port and compared with the native loop
  (`std::map` + `name ::= text\n`) transcribed in the fixture.
- **warnings, transcribed**: empty, one and three messages against the native
  join (`; `) and prefix.

| | |
|---|---|
| cases / values | **24 / 34, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |
| mutations | **12/12** (sort disabled, sort reversed, prefix tie-break, separator spacing, missing newline, key/text swap, failure prefix newline, dropped message, warning join, warning prefix space, warning newline, first-warning-only) |

## Scope

P7-09 stays PARTIAL. Removed from the coverage list: item 1. Remaining: PEG AST
helpers, `chars` pattern parsing, arena `resolve_refs`, the PEG builder helper
batch, and the grammar/PEG malformed-deep-large corpus.
