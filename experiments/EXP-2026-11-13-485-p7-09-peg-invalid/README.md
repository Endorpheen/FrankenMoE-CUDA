# EXP485: PEG schema passthrough and invalid-UTF-8 tracking (P7-09 fourteenth slice)

`until` consumes bytes that do not decode; the native engine records them as
invalid runs on the result and on every enclosing node. This slice ports that
bookkeeping together with the schema parser's pass-through.

## What was ported

`source/peginv.goose`, from `parser_executor`:

| decision | rule |
|---|---|
| schema | the child's result unchanged |
| until | scan to the first delimiter start (complete or an end-reaching prefix); undecodable bytes become invalid runs `(pos, len)`; lenient incomplete -> NEED_MORE before recording |
| sequence | concatenate child invalid runs on success and NEED_MORE; a failure yields none |
| rule/tag | copy the result's invalid runs into the node and return them on the result |
| dump | unchanged (invalid runs are not printed) |

Node invalid runs live in their own arena (`aninv`), separate from the result
runs (`invp`), because both grow during the same parse.

## Gate

`tests/peg_inv_fixture.cpp` builds trees with the real builder, encodes them by
walking the built arena, runs `common_peg_arena::parse` and compares:

- result type, start, end and the invalid run list `(pos, len)`;
- every AST node's rule/tag/span and invalid run list;
- the raw result for the failing-sequence cases (no wrapper masking).

| corpus | cases |
|---|---|
| tag over until, rule under tag over until, literal+until, until inside a rule, until then a failing literal (wrapped and root), schema over a literal, schema over until | 8 trees x inputs (ASCII, 1/2-byte invalid runs, truncated sequences) x starts 0..2 x {none,lenient} |
| total | **512 cases, 3 081 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |

Mutations 11/11 caught: until run recording, run length, lenient incomplete,
partial delimiter, end position, sequence success/failure invalid handling, node
copy, node run index, rule result runs, schema passthrough.

Two mutations needed extra corpus cases: a multi-byte invalid run (truncated 3-
byte sequence) and a failing sequence at the root (the enclosing rule/tag
otherwise drops the invalid list on failure, masking the mutation).

## Scope

P7-09 stays PARTIAL. Invalid-run propagation through `choice`/`repetition` and
`string`/`chars` (which never record runs), `build_grammar`, the schema parsers
and the JSON side remain.
