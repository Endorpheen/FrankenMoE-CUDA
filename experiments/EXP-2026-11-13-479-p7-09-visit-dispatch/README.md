# EXP479: visit() dispatch, rule map and $ref targets in Goose (P7-09 eighth slice)

This slice joins the pieces: the schema tree walk, the rule name policy, the
rule map with collision suffixing and `$ref` resolution, producing the whole
grammar for a nested schema.

## What was ported

`source/visit.goose`, from `visit`, `_add_rule`, `_add_primitive`,
`_resolve_ref`, `_generate_union_rule` and `_build_object_rule`:

| decision | rule |
|---|---|
| rule name | reserved -> name + `-`; empty -> `root`; otherwise the name |
| sub name | name + `-` when the name is non-empty |
| dispatch | ANY, REF, ANY_OF, OBJECT, STRING, BOOLEAN, NUMBER, NULL |
| primitives | the dependency graph (value -> object/array/string/number/boolean/null, object -> string/value, array -> value, string -> char, number -> integral/decimal) with the native "root" override |
| ANY | `_add_rule(rule_name, _add_primitive("value"))` — the alias, not the content |
| empty object + ANY additional | same alias pattern with `object` |
| object | property kv rules, the required chain, the optional alternatives with their recursive rest rules |
| anyOf | alternatives named `<name>-<i>` (or `alternative-<i>` for an empty name), joined by ` | ` |
| ref | fragment after the first `#`, `ref`-prefixed name, resolve only when the rule is absent and the ref is not resolving, unresolved -> empty rule and an error |
| map | sanitized keys, same name + same text reuses the entry, otherwise the first free decimal suffix |

The primitive rule **contents** are native data passed in as a table; the graph,
naming, dispatch and map are Goose.

Three implementation findings shaped the module:

1. The rule map and the scratch text cannot share one arena: an outer rule body
   built after an inner rule overwrites stored entries. The map now lives in
   `mb`, the scratch in `tb`, and the optional chain in `ob`.
2. A hand-built body must keep the scratch cursor in sync with its own end,
   otherwise the next allocation writes over the body start.
3. The optional chain is built backwards iteratively (the native recursion),
   which removed the slice aliasing that corrupted the references.

## Gate

`tests/visit_fixture.cpp` builds the same schema twice: as JSON for the native
`json_schema_to_grammar`, and as flat arrays for the Goose `convert`. The full
rule maps (every key and text) must be equal.

| corpus | cases |
|---|---|
| nested objects (2-3 levels), reserved names, weird names, empty object | 9 |
| required/optional mixes: two required, four optionals, anyOf optionals | 5 |
| refs: root ref, two refs to one object, a cycle A<->B, ref to anyOf, unresolved | 5 |
| anyOf: objects, nested in a property, anyOf of anyOf | 3 |
| primitives and simple objects (string, number) | 3 |
| total | **21 cases, 21 values, 0 failures** |
| ASan + UBSan (LeakSanitizer included) | 0 failures, rc 0 |

Mutations 15/15 caught: reserved dash, empty -> root, sub-name dash, ANY alias,
object alias, primitive deps, resolving guard, anyOf alt spelling, anyOf join,
kv spacing, required comma, rest key index, chain `?`->`*`, collision handling,
sanitizer runs.

Two mutations were removed as equivalent: using the last `#` (valid refs contain
one) and re-visiting an existing ref target (a deterministic re-visit produces
the same map; the check is a short-circuit, not a text decision).

## Scope

P7-09 stays PARTIAL: the PEG grammar parser is the remaining native piece; the
schema-to-GBNF converter is now Goose across EXP472-479.
