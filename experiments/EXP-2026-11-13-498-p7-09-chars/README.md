# EXP498: PEG chars pattern parsing in Goose (P7-09 27th slice)

`chars("a-z0-9_", …)` is the one builder call whose argument is parsed rather
than stored: a byte string becomes a list of codepoint ranges plus a negation
flag.

## What was ported

`source/chars.goose`, from `parse_char_classes`, `parse_char_class_char`,
`parse_hex_escape` and `is_hex_digit`:

| decision | rule |
|---|---|
| brackets | a leading `[` is dropped, then a trailing `]`, then a leading `^` sets negation — each only when the byte is present |
| escapes | `\xNN`, `\uNNNN`, `\UNNNNNNNN`; a malformed hex run falls back to the literal letter and consumes two bytes; `\n \t \r \\ \] \[`; any other escaped byte is literal; a trailing backslash is a literal backslash |
| plain bytes | each byte is its own codepoint, so a multi-byte UTF-8 literal yields one range per byte — the native quirk, reproduced |
| ranges | `-` starts a range only when a byte follows; otherwise it is a literal and the next byte starts a new entry |

An empty pattern or a lone `[` is undefined in the native code (`front()`/
`back()` on an empty string), so the corpus excludes them; the port treats both
as empty input.

## Gate

`tests/chars_fixture.cpp` builds each pattern with the real
`common_peg_parser_builder::chars`, reads `negated` and `ranges` from the built
`common_peg_chars_parser`, and compares them with the port:

| corpus | cases |
|---|---|
| ranges, classes, negation, `[]`, `]`, `[^]`, `a-`, `-a`, `a-b-c`, `[a-z-]`, `[-a]`, reversed `z-a`, all escape forms including malformed ones, `\q`, `^^`, `[^^]`, `[^a]`, control range, space, multi-byte `caf\u00e9` / `\u00e9` / a 3-byte literal | 42 |
| total | **42 cases, 158 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |
| mutations | **13/13** (bracket kept, trailing byte dropped, negation, range lookahead, range skip, `\n` letter, bad hex, `\x` width, unknown escape, ASCII mask, `\U` width, caret always stripped, trailing backslash) |

## Scope

P7-09 stays PARTIAL. Remaining: arena `resolve_refs`, the PEG builder helper
batch, and the grammar/PEG malformed-deep-large corpus.
