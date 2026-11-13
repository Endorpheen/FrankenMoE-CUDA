# EXP482: PEG matching engine, first slice (P7-09 eleventh slice)

The matching half of the PEG layer: literal, sequence, choice, repetition, any
and space, with the native result types, positions and NEED_MORE semantics.

## What was ported

`source/pegparse.goose`, from `parser_executor`:

| variant | rule |
|---|---|
| literal | byte compare; EOF -> FAIL at the start, or NEED_MORE(start, consumed prefix) in lenient mode; a mismatch -> FAIL(start, start) |
| sequence | left to right; the first failure wins and carries its end; NEED_MORE propagates; success ends at the last child's end |
| choice | the first non-failing option is returned unchanged; all failing -> FAIL(start, start) |
| repetition | stop before a child at end of input; stop on an empty match; bound by max; propagate NEED_MORE; success at the current position |
| repetition minimum | `count < min` -> FAIL(start, pos), or NEED_MORE(start, pos) when lenient and the input is exhausted |
| any | one UTF-8 codepoint with the native INCOMPLETE/INVALID split; lenient incomplete -> NEED_MORE, invalid -> FAIL |
| space | C-locale whitespace bytes, always succeeds |

These variants produce no AST nodes themselves (nodes come from rule/tag/schema
in a later slice), so the corpus asserts the native node list is empty and the
gate is result type plus start/end.

## Gate

`tests/peg_parse_fixture.cpp` builds 15 trees with the real builder, encodes the
same trees for Goose, and for every (tree, input, start, flags) compares
`common_peg_arena::parse` with `peg_parse`.

| corpus | coverage |
|---|---|
| trees | literal (short/long), two sequences, choice, four repetitions (unbounded/bounded/min), any, any+, space, space+literal, choice of sequences, repetition of a choice, repetition of a sequence |
| inputs | empty, ASCII prefixes, `aaaa`, UTF-8 2/3-byte, truncated and invalid UTF-8, spaces/newlines |
| starts | 0..3 (including past the end) |
| flags | none and lenient |
| total | **2 130 cases, 4 260 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |

Mutations 15/15 caught: literal EOF handling, literal prefix consumption, sequence
failure/NEED_MORE ends, choice swallowing NEED_MORE, repetition max off-by-one,
EOF child call, minimum check, lenient minimum inversion, UTF-8 incomplete/
invalid handling, 3-byte width, space set, continuation validation, repetition
NEED_MORE propagation.

## Scope

P7-09 stays PARTIAL. Still native in the PEG layer: lookahead (`and`/`not`),
`atomic`, `chars`, `string`, `until`, `ac`, rule/tag/schema matching with AST
nodes, invalid-UTF-8 tracking and `build_grammar`; plus the JSON side.
