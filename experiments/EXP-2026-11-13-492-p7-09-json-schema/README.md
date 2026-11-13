# EXP492: JSON-schema document builder in Goose (P7-09 twenty-first slice)

The JSON side of P7-09 starts here: the schema *reader* that turns a JSON
document into the `common_chat_schema` tree the converter and the PEG builder
consume. The JSON decoder itself stays native in this slice.

## What was ported

`source/schema.goose`, from `common_chat_schema_builder` in `common/json-schema.cpp`:

| decision | rule |
|---|---|
| dispatch | `$ref`, then `oneOf`/`anyOf`, then a `type` array expansion, then `const`, then `enum`, then the type name |
| implicit kinds | no type: `properties`/`additionalProperties` -> object, `allOf` -> all-of, `items`/`prefixItems` -> array, `pattern`/`minLength`/`maxLength`/`format` -> string, else any |
| object | `required` as a name set; `properties` in document order; `additionalProperties` true -> any, object -> built, false -> none; no `properties` and no `additionalProperties` -> implicit any |
| array | `items` wins over `prefixItems`; an array value is a tuple; absent `items` -> any; `minItems`/`maxItems` |
| string | `pattern`, `format` (date/time/date-time/uuid1..5), `minLength`/`maxLength` |
| integer | `minimum` (rounds up) else `exclusiveMinimum`+1; `maximum` (rounds down) else `exclusiveMaximum`-1 |
| format | by name, and `uuidN` for N in 1..5 |
| refs | only `#/...`; token walk with array indices; a document map that reserves the key before the target, so cycles terminate; every occurrence becomes its own ref node |
| errors | the native text `JSON schema error at <path>: <msg>`, with the path registered for every branch (`/properties/<name>`, `/items`, `/items/<i>`, `/oneOf`, `/allOf`, `/type/<i>`, `/additionalProperties`, and the ref text for ref targets) |

## Gate

`tests/schema_fixture.cpp` builds the same JSON document twice: as a flat tree
for the Goose builder, and through the native `common_chat_schema_from_json`.
It then compares

- the node count and, for every node, the kind and all kind-specific fields
  (ref text/target, children, const/enum values, integer bounds, string
  pattern/format/lengths, array items/bounds, object properties/required/
  additional);
- the document `$ref` map (ref text -> target node);
- for rejecting schemas, the exact exception message.

Native node ids follow a root-first traversal while the port's follow creation
order, so the fixture renumbers the port's tree by a DFS before comparing; that
is the only structural transformation applied.

| corpus | cases |
|---|---|
| accepted: primitives, empty, objects (required, additional, implicit), arrays (bounds, tuple, prefixItems, items priority), strings (pattern, every format), integers (bounds, fractional, exclusive), type arrays, anyOf/oneOf/allOf, const/enum, refs (simple, chain, cycle, shared target, self ref, ref into an array) | 55 |
| rejected: non-object, unknown type, empty enum/type array, wrong types, negative/fractional counts, bad anyOf/allOf, ref errors, and nested error paths inside properties/anyOf/allOf/type/items/tuple/additionalProperties/ref | 30 |
| total | **85 cases, 541 values, 0 failures** |
| ASan + UBSan | 0 failures, rc 0 |

Mutations 23/23 caught: root path, double-as-integer, negative count, rounding
direction, uuid5, oneOf/anyOf, type override, const key, empty enum,
additionalProperties inference, required names, additional any node, implicit
object any, items/prefixItems priority, swapped item bounds, pattern, format,
both exclusive shifts, ref map reuse, pending targets, empty type array, unknown
type.

## Scope

P7-09 stays PARTIAL: the JSON decoder (`common/json.cpp`), the deep/large input
corpus and the PEG schema parsers that consume this tree remain.
