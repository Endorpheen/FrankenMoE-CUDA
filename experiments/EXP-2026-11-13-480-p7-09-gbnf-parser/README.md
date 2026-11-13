# EXP480: GBNF rule-text parser in Goose (P7-09 ninth slice)

The grammar text front end: rule names, the symbol table, literals with escapes,
character classes, references, groups and repetitions. This slice ports it and
gates acceptance, the exact error message, the symbol ids and the full element
structure against the real `llama_grammar_parser`.

## What was ported

`source/gbnf.goose`, from `src/llama-grammar.cpp`:

| decision | rule |
|---|---|
| spaces | space/tab, `#` comments, newlines only where the native allows them (`is_nested` at the call site) |
| names | `[A-Za-z0-9-]+` |
| symbols | insertion-order ids; generated ids are `<rule>_<next_id>` |
| escapes | `\xNN`, `\uNNNN`, `\UNNNNNNNN`, `\t\r\n`, `\\ " [ ] -`; other escapes error |
| literals | `"..."` with `parse_char`, EOF before the closing quote is an error |
| classes | optional `^`, alternate chars (`CHAR_ALT`), ranges (`CHAR_RNG_UPPER`) when `-` is not before `]` |
| any | `.` -> `CHAR_ANY` |
| refs | rule references through the symbol table |
| groups | a synthesized rule named `<rule>_<id>`, a `RULE_REF` in the parent, `)` required |
| repetitions | `* + ? {n,m}` with the native rewrite: `S{m,n}` expands to `m` copies plus generated `S'` rules chaining the remainder, `*`/`+` self-reference, the 2000-rule guard |
| validation | empty rules and undefined references after the whole text |
| errors | the native message text plus ` at <remaining source>` |

Tokens (`<...>`, `!<...>`) need a vocabulary and are outside this corpus; the
native parser would dereference a null vocab there.

## Gate

`tests/gbnf_fixture.cpp` links the real `llama_grammar.cpp` object from the flint
build tree and compares:

- acceptance (the `parse()` bool);
- on errors, the printed message extracted from the `parse: error parsing grammar:`
  line (including the remaining-source suffix);
- on success, the full `symbol_ids` map and every rule's element list
  `(type, value)`, rule by rule.

| corpus | cases |
|---|---|
| valid: literals, escapes, classes, ranges, refs, alternations, groups, nested groups, `.`, comments, unicode, multi-rule, trailing `\|` | 28 |
| invalid: missing `::=`, unterminated literal/class, unknown/broken escapes, undefined refs, `*` without an item, `{...}` errors, missing `)`, trailing text, repetition guard | 18 |
| total | **46 cases, 322 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |

Mutations 18/18 caught: unknown escape accepted, `\u` width, short hex, EOF
literal, class negation, class alternate type, ranges, ref length, generated id
separator, ALT markers, ref type, dot type, zero-min truncation, star
self-reference, optional count, undefined-ref validation, `::=` check, trailing
text.

## Scope

P7-09 stays PARTIAL: the PEG parser (`common/peg-parser.cpp`) and the JSON side
of the criterion remain; the GBNF text parser is now Goose. Tokens that need a
vocabulary are excluded and documented.
