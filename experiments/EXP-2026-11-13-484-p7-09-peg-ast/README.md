# EXP484: PEG rule/tag AST accumulation in Goose (P7-09 thirteenth slice)

The AST half of the PEG layer: `rule` and `tag` wrap a non-failing child into a
node, sequences/repetitions concatenate nodes, choice returns the chosen option's
nodes, atomic drops them on NEED_MORE, and the arena dump prints every node with
its subtree.

## What was ported

`source/pegast.goose`, from the `parser_executor` rule/tag/atomic branches and
`common_peg_ast_arena`:

| decision | rule |
|---|---|
| rule/tag | parse the child; on failure return it unchanged; otherwise create one node with the rule or tag name, start, end, `input[start..end]` text, the child node ids, and partial = NEED_MORE; return exactly that node |
| sequence | nodes concatenated in child order; a failure returns no nodes; NEED_MORE carries the accumulated nodes |
| choice | the chosen option's type/positions/nodes unchanged |
| repetition | successful children's nodes concatenated; NEED_MORE carries them |
| atomic | the child's result, with its node list cleared on NEED_MORE (the nodes already added to the arena stay) |
| ids | creation order |
| dump | for every node in creation order: `NODE <id> (rule r)? (tag t)? ['<text>']` and its children recursively, indented two spaces |

Schema/ref matching, invalid-UTF-8 tracking and `build_grammar` are outside this
slice. The fixture resolves a `ref` root to its rule before encoding (native
`resolve_refs` already rewrites container children).

## Gate

`tests/peg_ast_fixture.cpp` builds trees with the real builder, walks the built
arena (refs already resolved) into flat arrays, runs `common_peg_arena::parse`
and compares with `peg_parse_ast`:

- result type, start, end and the result node ids;
- every AST node: id, rule, tag, start, end, text, children, is_partial;
- the full `common_peg_ast_arena::dump()` string.

| corpus | cases |
|---|---|
| tag over literal, tag over rule, nested tags, tag over sequence+tag, repetition of tags in a rule, two sibling tags, a ref root rule, atomic over a tag (NEED_MORE node drop) | 8 trees x 8 inputs x starts 0..2 x {none,lenient} |
| total | **368 cases, 3 986 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |

Mutations 14/14 caught: node start, rule/tag slot, partial flag, text, child
recording, result node list, sequence concatenation, atomic NEED_MORE, failing
rule node, dump indent, rule name, tag name, text, child traversal.

## Scope

P7-09 stays PARTIAL. Still native: schema matching nodes, invalid-UTF-8 tracking
on results and nodes, `build_grammar`, the schema parsers and the JSON side.
