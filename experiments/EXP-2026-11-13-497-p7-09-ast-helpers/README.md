# EXP497: PEG AST helpers in Goose (P7-09 26th slice)

The query surface over a parse result: find a node by tag or rule with a depth
limit, walk the tree in order, and render a node's text with its invalid UTF-8
runs replaced.

## What was ported

`source/asthelp.goose`, from `common/peg-parser.cpp`:

| function | rule |
|---|---|
| `find_by_tag`, `find_by_rule` | children in order; a match returns at once; otherwise the child is searched when `max_depth > 1`, with the depth decremented. The node the call starts from never matches itself. |
| `visit` | pre-order DFS: the node, then each child in order |
| `sanitized_text` | no invalid runs -> the text verbatim; otherwise each run becomes U+FFFD and the bytes between runs are copied unchanged |
| `parse_result_type_name` | `fail`, `success`, `need_more_input`, `unknown` |

## Gate

`tests/ast_fixture.cpp` builds five PEG trees (tag over literal, tag over a rule
over `until`, three nested tags, a repetition of tags, two tags in a rule) and
parses ten inputs, including lone 0xff, truncated 2- and 3-byte sequences and
valid UTF-8. For every produced AST it encodes the arena flat and compares with
the native methods:

- `find_by_tag` and `find_by_rule` from **every node**, for every tag/rule name
  present plus a miss, at depths 0, 1, 2, 3 and 5;
- `visit` from every node, comparing the full id sequence;
- `sanitized_text` for every node;
- the four result-type names.

| | |
|---|---|
| cases / values | **55 / 3 085, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |
| mutations | **13/13** (extra level, single level, shallow-vs-deep preference, self-match, rule-vs-tag field, post-order, children skipped, raw bytes kept, two-byte U+FFFD, cursor step, tail dropped, two name spellings) |

The invalid-run pool is indexed in runs, not slots; the first draft mixed the two
and the gate caught it on the first input with a recorded run.

## Scope

P7-09 stays PARTIAL. Coverage item 1 is closed. Remaining: `chars` pattern
parsing, arena `resolve_refs`, the PEG builder helper batch, and the
grammar/PEG malformed-deep-large corpus.
