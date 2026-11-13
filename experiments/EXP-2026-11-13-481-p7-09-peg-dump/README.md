# EXP481: PEG parser-tree dump in Goose (P7-09 tenth slice)

The PEG layer in this repository is built programmatically by
`common_peg_parser_builder`; `common_peg_arena::dump` renders the resulting tree.
This slice ports that rendering, with the exact visited-set semantics.

## What was ported

`source/pegdump.goose`, from `common_peg_arena::dump` / `dump_impl`:

| kind | text |
|---|---|
| epsilon/start/end/any/space | `Epsilon`, `Start`, `End`, `Any`, `Space` |
| literal | `Literal(<text>)` |
| sequence/choice | `Sequence(a, b, ...)` / `Choice(a, b, ...)` |
| repetition | `Repetition(child, min, max\|unbounded)` |
| and/not/atomic | `And(child)`, `Not(child)`, `Atomic(child)` |
| chars | `CharRepeat(pattern, min, max\|unbounded)` |
| string | `String(<delimiter>)` |
| until | `Until(d1 \| d2 \| ...)` |
| rule/ref | `Rule(name, child)`, `Ref(name)` |
| tag | `Tag(tag, <fresh dump of child>)` |
| gbnf/ac | `Gbnf(grammar, child)`, `Ac(d1 \| d2, child)` |

Two details are reproduced exactly:

1. the visited set is global to one `dump()` call and ids are never removed, so a
   shared subtree prints `[cycle]` the second time;
2. `Tag` re-dumps its child with a **fresh** visited set and then restores the
   outer one.

At build time native `resolve_refs` rewrites container children to the resolved
rule, so a ref inside a tree dumps as the target `Rule(...)`; a ref dumped on its
own still prints `Ref(name)`. The corpus keeps refs only in the direct-dump case.

## Gate

`tests/peg_fixture.cpp` builds the same spec tree twice - with the real builder
and as flat arrays for `peg_dump` - and compares `common_peg_arena::dump()` byte
for byte.

| corpus | cases |
|---|---|
| all leaf kinds in one sequence, shared subtree, choice of sequences, repetitions, lookaheads/atomic, tags (both visited directions), gbnf, ac with three delimiters, nested choice/sequence, a rule by name, a ref by name, until with two delimiters and unbounded chars | 13 |
| total | **13 cases, 13 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |

Mutations 14/14 caught: no visited recording, cycle check off, Tag without reset,
Tag without restore, unbounded as a number (twice), sequence spacing, choice
name, string parentheses, until spacing, ac spacing, rule comma, epsilon
spelling, ref name.

## Scope

P7-09 stays PARTIAL: this slice covers PEG **rendering** only. The matching
engine (`common_peg_arena::parse`, AST nodes, lookahead/repetition semantics),
schema parsers and the JSON side remain native.
