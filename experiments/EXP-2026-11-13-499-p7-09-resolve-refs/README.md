# EXP499: PEG ref resolution in Goose (P7-09 28th slice)

`build()` ends with `resolve_refs()`: every ref child becomes the id of the rule
it names, and a ref root is flattened one level.

## What was ported

`source/resolve.goose`, from `resolve_ref` and `resolve_refs`:

| decision | rule |
|---|---|
| visitation | every parser in id order |
| sequence, choice | every child is resolved |
| repetition, and, not, rule, atomic, tag, gbnf, ac, schema | the single child is resolved |
| childless kinds | untouched |
| root | a ref root is flattened one level |
| unknown name | an error carrying that name (`Rule not found: <name>` natively) |

## Gate: the native resolver on the same graph

`resolve_ref` is private, but `get(id)`, `set_root()` and `resolve_refs()` are
public, so the fixture builds a tree with the real builder, **injects** an
unattached ref parser into a container child, encodes the arena flat, runs the
port, then calls the native `resolve_refs()` on the same arena and encodes it
again. The port's child ids, kinds and root must equal the native result.

| corpus | cases |
|---|---|
| injection into sequence (2 slots), choice (2 slots), repetition, peek, negate, atomic, tag, gbnf, ac, schema, rule | 22 |
| root flattening | 1 |
| unknown name: the native build throws `Rule not found: missing`; the port returns the error with that name | 1 |
| total | **24 cases, 271 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |
| mutations | **12/12** (sequences skipped, choices skipped, single-child kinds skipped, schemas skipped, rules skipped, root not flattened, child left alone, unknown ignored, missing name not reported, only the first parser visited, every child rewritten, first rule always wins) |

Comparing names by string id rather than text is equivalent under this encoding
(a single interner), so that mutation is not kept.

## Scope

P7-09 stays PARTIAL. Remaining: the PEG builder helper batch, and the
grammar/PEG malformed-deep-large corpus.
