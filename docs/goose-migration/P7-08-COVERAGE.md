# P7-08 coverage: chat templates, Jinja and reasoning parsing

Criterion: "All shared Jinja/template/filter/parser/tool/reasoning logic Goose;
Apex golden prompt bytes and structured output exact; no minja arithmetic or
parsing wrapper left."

Status: **PARTIAL**. This document assigns an owner to the template stack so the
remaining work is a list. "Goose" means a differential gate against the native
code; "pending" means still native.

## Modules

| module | lines | owner |
|---|---|---|
| `common/jinja/lexer.cpp`, `lexer.h` | 361 + 157 | **Goose (EXP502)**: tokens, normalization, trim, escapes, positions, error text |
| `common/jinja/parser.cpp`, `parser.h` | 619 + 21 | **Goose (EXP503)**: token stream to the 30 node shapes, precedence chain, statements, arguments, positions and error text |
| `common/jinja/runtime.cpp`, `runtime.h` | 1 015 + 777 | pending — statement and expression execution, filters, tests |
| `common/jinja/value.cpp`, `value.h` | 1 758 + 765 | **Goose (EXP504, EXP505)**: the value model (types, conversions, flags, both equalities, `value_compare`, `value_to_string_repr`) and JSON output (`value_to_json`, `float_repr`, `float_json`). Pending: `get_builtins` (built-ins/filters per type) and the arithmetic operators |
| `common/jinja/string.cpp`, `string.h` | 213 + 61 | pending — input marking and origin metadata |
| `common/jinja/caps.cpp`, `caps.h` | 592 + 40 | pending — capability checks |
| `common/jinja/utils.h` | 149 | Boundary — `peak_source` and the FNV hasher are utilities; `peak_source` is already reproduced by EXP502 for lexer errors |
| `src/llama-chat.cpp` | — | pending — template application and message conversion |
| `common/chat.cpp`, `chat.h`, `chat-*.cpp` | — | pending — chat parsing, tool calls, reasoning extraction |
| `common/parsers/**` | ~3 360 | pending — per-model output parsers |

## Slice plan

Eight slices; `tasks.json` mirrors this count (`slices_total`/`slices_done`).

1. Lexer (EXP502) — done.
2. Parser: token stream to program, with the exact node shapes and error text — done (EXP503).
3. Value model: types, conversions, flags, equality, `value_compare`, repr — done (EXP504).
4. JSON output: `value_to_json`, `float_repr`, `float_json` — done (EXP505).
5. Builtins and filters: `func_args`/`value_func`, `global_builtins`, the
   per-type tables, and the arithmetic operators — in progress. EXP506 ports
   `func_args` and the int/float tables (51 cases, 14/14 mutations, final -O2
   gate); bool/none/undefined, string, array/object and `global_builtins` are
   still native.
6. Runtime: statement execution and expression evaluation (`for`/`if`/`set`/
   macros), filters/tests dispatch.
7. String and caps: input marking, origin metadata, concatenation rules,
   capability checks.
8. Chat layer and acceptance: `llama-chat.cpp`, `common/chat*.cpp`, the
   per-model parsers, then the Apex golden prompt bytes and structured output.

## Boundaries allowed by the criterion

| boundary | why |
|---|---|
| `common_json` as the JSON-to-value translation point | the document type is P7-09's boundary; the translation policy is Goose |
| libc `isdigit`/`isalnum`/`isspace` semantics | byte classification, reproduced explicitly in Go (EXP502) |
| `strtod`/`stoll` number conversion | correctly rounded decimal conversion, one extern (`gs_json_strtod`) in the parser slice |
| the virtual `execute` methods on the AST nodes | execution belongs to slice 4 (runtime); EXP503 owns the node shapes and the parse decisions |
| the model vocabulary for special-token detection | an external input, not a conversion decision |
