# EXP-2026-11-14-570 — JSON Schema to GBNF (P7-08 slice 13)

Native counterpart: `common/json-schema-to-grammar.cpp` (converter part).
Base commit for the port: `e1238277`. The canonical runtime source is unchanged;
this experiment owns `goose/json/format-grammar.goose`.

## What this slice covers

The schema converter behind `json_schema_to_grammar`: the document walk
(`$ref`, `oneOf`/`anyOf`, type arrays, `const`, `enum`, implicit kinds when no
type is present), object required/properties/`additionalProperties` policy,
array `items`/`prefixItems` precedence and tuples, string `pattern`/`format`
and length bounds, integer `minimum`/`maximum` with exclusive folding and inward
rounding, the `uuid1..5` formats, the `#/...` ref walk with a document map that
reserves keys so cycles terminate, the allOf merge (optional anyOf alternatives,
required components, enum intersection), and the exact
`JSON schema error at <path>: <msg>` text with the native path forms.

The rule-text sorting, `format_grammar`, `check_errors` and the wrapper prefix
were already canonical from EXP496; the two files were merged into one, and the
P7-09 experiment is now marked canonical so the sync does not create a second
`format-grammar` slot.

## Gate

`tests/build.sh` builds `source/format-grammar.goose` with `goose --standalone`,
compiles the generated C at `-O0`, links `tests/fixture.cpp` against the
`json_schema_to_grammar` oracle from `upstream/llama.cpp` and compares the full
rule map for 30 schemas from `tests/cases.md`.

```
FG573_SCHEMA cases=30 values=182 failures=0
```

`values=182` is the total number of oracle rules over the 30 schemas. The
oracle version matters: the number can change with another llama.cpp base, while
`failures` must stay 0.

## Mutations

`tests/mutate.py` holds 12 mutations in two halves; each rewrites the converter,
rebuilds and reruns the fixture.

```
python3 tests/mutate.py --half first    # 6/6 caught
python3 tests/mutate.py --half second   # 6/6 caught
```

## Exports

Base canon exports 4 (`schema_build`, `grammar_text`, `conversion_failed`,
`conversion_warning`); the slice adds `json_schema_convert` — 5 total, no
missing and no duplicated names. `goose check` typechecks with 264
specializations.

## Recorded limits

- The gate runs at `-O0`, the repository template expects `-O1` for gates.
- `values=182` is tied to the llama.cpp base; `failures` is the stable signal.
- Mutation 3 (pattern wrapper) is caught as a runtime out-of-bounds error, not
  as a rule-map diff; the fixture treats a crash as a failure.

## Files

- `source/format-grammar.goose` — the converter (canonical file).
- `tests/fixture.cpp` — oracle comparison over 30 schemas.
- `tests/build.sh` — build and gate; `FG_LLAMA`/`FG_BIN` select the base.
- `tests/mutate.py` — 12 mutations in two halves.
- `tests/cases.md` — the schema corpus.
- `tests/FIX.md` — the signature mismatch that blocked integration and the six
  converter defects the corrected gate found.
