# P7-09 inventory: JSON, schema, PEG and grammar parsing are native

Criterion: "JSON/schema/PEG/grammar parsing/rendering and validation owned by Goose; malformed/deep/large input..."

Measured on the merged candidate with the same marker used elsewhere:

| surface | location | Goose marks |
|---|---|---|
| GBNF parsing and grammar acceptance (`llama_grammar_*`, 21 + 8 symbols) | `libllama` / `llama-grammar.cpp.o` | **0** `gg_function_` |
| `json_schema_to_grammar` (5 symbols) | `libllama-common` | 0 in the whole library |
| sampler-side grammar filtering | native, as above | 0 |

So every parsing surface named by this item is native code. Behavioural parity is easy to demonstrate -- and meaningless as migration
evidence -- because both lines run the same implementation. The grammar-constrained generation case in EXP351 is exactly that case, and its
README now says so.

## What the item would require

Porting `llama-grammar.cpp` (a PEG-style grammar with its own parser, stack machine and state caching) plus the JSON-schema-to-GBNF
conversion, both to Goose. That is a project the size of the minja port in P7-08, and neither has been started.

## What can be said honestly today

- Malformed, deep and large grammar inputs have not been differentially tested, because a differential would compare native against native.
- The one property worth having is a regression check that the merged build still handles grammar identically; EXP351 provides that and now
  labels it correctly.
