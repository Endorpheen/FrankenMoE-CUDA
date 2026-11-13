# EXP495: schema introspection and the P7-09 coverage audit

Two things in one commit: the audit that assigns an owner to every function of
the five P7-09 files (`docs/goose-migration/P7-09-COVERAGE.md`), and the largest
gap it found — the schema query layer.

## What was ported

`source/schemaint.goose`, from `common/json-schema.cpp`:

| function | rule |
|---|---|
| `json_type` | a JSON literal's value_type: null, boolean, integer, number, string, array, object |
| `value_types` | `type_set` bitmask: any -> all; number -> number|integer; array/tuple -> array; const/enum from the literal types; ref follows the target with a visited set; anyOf union; allOf intersection starting from all |
| `may_be_string` | string -> true; const/enum by the literal type; ref follows the target with a visited set; anyOf any true; allOf every non-any child true and at least one |
| `kind_name`, `type_name` | the diagnostic identifier tables |

## Gate

`tests/schema_int_fixture.cpp` builds the schema natively with
`common_chat_schema_from_json`, encodes it flat, and compares, for **every node**
of the tree, the native `value_types()` mask and `may_be_string()` with the
ported functions, plus the two name tables:

| corpus | coverage |
|---|---|
| 34 schemas | every kind, all const literal types, enum mixed and container values, anyOf/allOf (including empty and any-only), refs, ref cycles, `type` arrays, tuples, object properties |
| assertions | one `value_types` and one `may_be_string` per node (163 values), 21 names |
| total | **56 cases, 163 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |

Mutations 15/15 caught: the any mask, number/integer, both ref cycle guards, the
visited pop, the allOf seed, anyOf union vs intersection, enum contribution,
const type, the allOf any-child skip, the allOf string check, the enum scan, both
name tables, and the tuple type.

Two corpus gaps were found by surviving mutations and fixed: an enum whose string
is not the first value, and an `allOf` of two refs to the same target (which is
the only shape where the visited pop is observable, because a union of identical
targets hides it).

## Scope

The audit keeps P7-09 PARTIAL. Remaining, in order: `check_errors` +
`format_grammar`; the PEG AST helpers; `chars` pattern parsing; arena
`resolve_refs`; the PEG builder helper batch; and the grammar/PEG malformed-deep-
large corpus.
