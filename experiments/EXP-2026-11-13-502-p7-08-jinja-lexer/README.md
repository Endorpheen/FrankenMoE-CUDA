# EXP502: Jinja lexer in Goose (P7-08 first slice)

The first slice of P7-08: the template source becomes tokens. Everything above
it (parser, runtime, values, filters) still speaks the native token stream, so
this slice fixes the lexer contract exactly.

## What was ported

`source/lexer.goose`, from `jinja::lexer::tokenize`:

| decision | rule |
|---|---|
| normalization | `\r\n` and `\r` become `\n`; then one trailing newline is dropped if the ORIGINAL source ended in `\n` (so a source ending in `\r` keeps the produced newline) |
| mapping table | trimmed forms `{%- -%} {{- -}}` first, then `{% %} {{ }}`, single characters, comparisons (`<= >= == != < >`), arithmetic (`+ - ~ * / %`), then `=` |
| brace depth | `}}` is a `close_expression` only while `curly_bracket_depth == 0`; `{{` resets the depth, `{` adds one, `}` removes one |
| text blocks | `lstrip_blocks` and `trim_blocks` on by default, plus the rstrip/lstrip of the neighbouring block from the previous token's tail |
| comments | `{# ... #}` with the content as the token value |
| unary | `-`/`+` is unary unless the previous token is an identifier, number, string, `)` or `]`; a unary sign grabs the numeric literal that follows |
| literals | numbers with an optional `.digits` fraction; strings with `\n \t \r \b \f \v \\ \' \" \xNN`; identifiers `[A-Za-z0-9_]+` |
| errors | `lexer: <msg>` followed by `peak_source` over the ORIGINAL source: 40 bytes either side, newlines as U+21B5, a caret under the position |

The lexer tokenizes byte positions of the normalized source but builds errors
from the original source, which is the native behaviour and is what the gate
checks.

## Gate

`tests/lexer_fixture.cpp` runs the native `jinja::lexer::tokenize` and the port
on the same source and compares:

- on success: the token count, and for every token its type, value bytes and
  `pos`, plus the normalized source the native result carries;
- on failure: the exact exception text, byte for byte.

| corpus | cases |
|---|---|
| hand-written: normalization, text and trim behaviour, expressions, statements, comments, object literals, every operator, unary versus binary, all escapes, UTF-8, vertical tab, raw blocks, and 11 rejected inputs (unexpected character, unterminated string/escape, incomplete/invalid hex, unknown escape, unclosed comment) | 100 |
| generated: 5 000 expressions, 200 000-byte text, 50 000-byte identifier, 50 000-byte string, 5 000 trimmed statement lines, 500 nested braces, 2 000 text/expression pairs | 7 |
| total | **107 cases, 131 025 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |
| mutations | **20/20** (CRLF, trailing newline, depth reset, depth guard, text stop, lstrip, trim, both trim directions, explicit dash, unary classification, newline escape, hex width, unknown escape, fraction, underscore, vertical tab, caret column, comment opener, text start) |

Two real porting bugs were caught by the gate: the two neighbour-trim
directions were swapped (rstrip must strip the text's start, lstrip its end),
and unary tokens carried the position after the sign instead of the sign.

Three corpus gaps were found by surviving mutations and filled: a new
expression that must reset the depth, a `}}` that appears while the depth is
non-zero, and the silent dash after `{%-`/`{{-`.

## Scope

P7-08 stays PARTIAL: `docs/goose-migration/P7-08-COVERAGE.md` lists the remaining
modules. Next slice: the parser (tokens to `jinja::program`).
