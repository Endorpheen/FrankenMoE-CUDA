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
| `common/jinja/value.cpp`, `value.h` | 1 758 + 765 | pending — primitive types, object/array, built-ins |
| `common/jinja/string.cpp`, `string.h` | 213 + 61 | pending — input marking and origin metadata |
| `common/jinja/caps.cpp`, `caps.h` | 592 + 40 | pending — capability checks |
| `common/jinja/utils.h` | 149 | Boundary — `peak_source` and the FNV hasher are utilities; `peak_source` is already reproduced by EXP502 for lexer errors |
| `src/llama-chat.cpp` | — | pending — template application and message conversion |
| `common/chat.cpp`, `chat.h`, `chat-*.cpp` | — | pending — chat parsing, tool calls, reasoning extraction |
| `common/parsers/**` | ~3 360 | pending — per-model output parsers |

## Slice plan

1. Lexer (EXP502) — done.
2. Parser: token stream to program, with the exact node shapes and error text — done (EXP503).
3. Value: primitive types, comparison/arithmetic/conversion, object and array,
   then the built-in function and filter table.
4. Runtime: statement execution, expression evaluation, `for`/`if`/`set`/macros,
   filters and tests.
5. String: input marking, origin metadata, concatenation rules.
6. Caps: capability checks.
7. Chat layer: `llama-chat.cpp` template application, then `common/chat*.cpp`
   parsing, then the per-model parsers.
8. Final acceptance: Apex golden prompt bytes and structured-output exactness.

## Boundaries allowed by the criterion

| boundary | why |
|---|---|
| `common_json` as the JSON-to-value translation point | the document type is P7-09's boundary; the translation policy is Goose |
| libc `isdigit`/`isalnum`/`isspace` semantics | byte classification, reproduced explicitly in Go (EXP502) |
| `strtod`/`stoll` number conversion | correctly rounded decimal conversion, one extern (`gs_json_strtod`) in the parser slice |
| the virtual `execute` methods on the AST nodes | execution belongs to slice 4 (runtime); EXP503 owns the node shapes and the parse decisions |
| the model vocabulary for special-token detection | an external input, not a conversion decision |
