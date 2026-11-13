# EXP477: _not_strings in Goose (P7-09 sixth slice)

A typed `additionalProperties` needs a key rule that excludes the names already
declared. The native converter builds it with `_not_strings`: a trie over the
property names and a recursive GBNF exclusion expression. This slice ports the
trie and the text.

## What was ported

`source/notstrings.goose`, from `_not_strings` and `common_trie`:

| decision | rule |
|---|---|
| trie | names inserted as Unicode codepoints; children kept sorted by codepoint (the native `std::map` order); duplicate names reuse the existing path |
| empty name | an empty word marks the root node as a pattern |
| per child | `[c]` then either a nested `( … )` when the child has children, or ` char+` at a leaf |
| alternatives | separated by ` | ` |
| sibling rejection | ` | [^"<all child chars>] char*` after the children |
| wrapper | `["] ( ` + visit(root) + ` )` + `?` when the root is not a pattern + ` ["]` |

UTF-8 decoding/encoding follows the valid 1-4 byte forms; the corpus uses valid
UTF-8, and invalid bytes fall back to themselves. The `char` primitive name is
passed in; the schema walk and the rule map stay native.

## Gate

`tests/notstrings_fixture.cpp` builds real objects with a typed
`additionalProperties`, runs the native `build_grammar` and compares the
`additional-k` rule text (which is exactly `_not_strings` over the property
names) with the Goose output.

| corpus | cases |
|---|---|
| fixed sets: single name, prefixes, shared prefixes, unicode (2- and 3-byte), empty name, empty + name, duplicates, punctuation | 16 |
| 60 random sets of 1-6 names over a small alphabet incl. `é` | 60 |
| total | **76 cases, 76 values, 0 failures** |
| ASan + UBSan over module and fixture | 0 failures, rc 0 |

Mutations 14/14 caught: unsorted children (50), 3-byte decode (1), 2-byte encode
(25), trailing `?` (76), empty-word root (2), rejects (63), exclusion quote (75),
leaf `+`→`*` (75), nested wrapper (54), alternation spaces (62), root opener (76),
closing bracket (76), child reuse (32), high bytes (25).

Two mutations were initially equivalent and were replaced: the standalone
`utf8_put` was dead code after the encoder was inlined, and `found>=1` happens to
match every real node index, so neither exercised the gate.

## Scope

P7-09 stays PARTIAL. allOf, the `$ref` target walk, the visit dispatch/recursion,
the primitive rule texts and the PEG parser are still native; `_not_strings` is
now Goose.
