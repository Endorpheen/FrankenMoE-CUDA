# EXP472: JSON-schema-to-GBNF shaping decisions in Goose (P7-09 first slice)

The schema converter emits GBNF text; the shapes it chooses are the decisions this
slice ports. The text itself and the rule-name map stay native.

## What was ported

`source/schema.goose`, from `build_repetition` and `_build_object_rule`:

| decision | rule |
|---|---|
| repetition kind | max 0 → empty; (0,1) → `?`; min 1 without max → `+`; min 0 without max → `*`; otherwise `{min,max}`; with a separator always the recursive form |
| separator recursion | inner minimum is `min-1` (0 when min is 0) and a min of 0 wraps the whole item as optional |
| object split | required properties in declaration order first, then one alternative branch per optional property |
| additionalProperties | counted as one extra optional branch (`*`) |
| empty object | no required and no optional properties → the empty-object form |
| comma prefix | the separator before the optional group exists only when both groups are non-empty |

## Gate

`tests/schema_fixture.cpp` transcribes the native branch selection and compares,
including 300 randomised property sets.

| corpus | cases |
|---|---|
| repetition matrix (5 minima x 5 maxima x separator on/off) | 50 |
| separator recursion for the separator form | 13 |
| 300 random property/required sets with additionalProperties on/off | 300 |
| total | **369 cases, 1 569 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |

Mutations 8/8 caught: zero maximum no longer empties (10), optional form needs
min 1 (4), plus form for min 0 (1), separator keeps min (16), wrapper dropped (3),
required/optional swapped (729), additionalProperties not counted (306), comma
prefix ignores optionals (9).

## Scope

P7-09 stays PARTIAL: the rule text emission, pattern/format handling, `$ref` and
the PEG grammar parser are still native. This slice covers the shaping decisions
that the converter makes before writing text.
