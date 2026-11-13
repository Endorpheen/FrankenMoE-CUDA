# EXP478: _visit_all_of in Goose (P7-09 seventh slice)

`allOf` merges its components into one object or one enum intersection before
the rule text is assembled. This slice ports that merge and checks it against the
real converter through a pair of native runs.

## What was ported

`source/allof.goose`, from `_visit_all_of` and `add_component`:

| decision | rule |
|---|---|
| top level | an anyOf expands its alternatives with `is_required = false`; every other top-level component starts with `true` |
| ref | follows its target with the same `is_required`; a ref whose target is an anyOf is ignored (native `add_component` has no anyOf branch) |
| properties | appended in traversal order, duplicates included |
| required | a set of names: a name is required when any occurrence was visited as required, and every occurrence of that name follows |
| enum counting | one count per rendered constant `format_literal(dump)`, aggregated by text |
| intersection | texts whose count equals the number of **top-level children**, in `std::map` (byte-wise) order |
| winner | a non-empty intersection produces the enum; otherwise the object build |

The schema walk, the rule map and the object/enum text stay native; the merge
emits property/required/enum lists for the caller.

## Gate: two native runs

`tests/allof_fixture.cpp` builds the real `common_chat_schema_all_of` tree and
takes its native root body. Then it feeds the Goose merge result into a second
native schema — a flat object with exactly the merged properties/required flags,
or an enum with the intersected values — and compares the two root bodies. If
the merge is wrong, the flat schema produces a different body.

| corpus | cases |
|---|---|
| objects: duplicates, order permutations, empty, empty property name | 8 |
| enums: intersections, disjoint, duplicate values, sorted order, numeric, boolean, escaped/unicode | 10 |
| anyOf: optional alternatives, anyOf of enums, anyOf object+enum (enum wins), required set promotion | 5 |
| refs: to objects, to enums, to anyOf (ignored), object + ref | 4 |
| total | **24 cases, 25 values, 0 failures** |
| ASan + UBSan over module and fixture | 0 failures, rc 0 |

One extra value checks the merged required flags directly. The native object
builder re-applies the required **name set**, so a flat object cannot express
per-occurrence flags; without the direct check the promotion pass is masked by
the oracle (this was found by a surviving mutation).

Mutations 16/16 caught: anyOf required (3), anyOf skipped (5), ref required (2),
ref ignored (3), required-set promotion (1, via the direct check), every
occurrence required (3), enum `!=` (12), extra child (10), unsorted (1),
descending (2), no aggregation (8), prepended properties (6), empty intersection
(4), byte order (2), top list ignored (7), one occurrence per name (9).

## Scope

P7-09 stays PARTIAL. Still native: the `$ref` target walk outside allOf, the
visit dispatch/recursion, the primitive rule texts and the PEG parser.
