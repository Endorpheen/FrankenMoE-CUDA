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
| `common/jinja/runtime.cpp`, `runtime.h` | 1 015 + 777 | **Goose (EXP507, EXP508, EXP509)**: expression evaluation and operators (EXP507); `program`, `if`, `set` including the block form and tuple unpacking, the output path and the context binding order (EXP508); `for` with its filter and per-iteration scope, `break`/`continue` consumption, the loop object, member access and tuple-unpack errors (EXP509); `macro` definition, calls, positional parameters, defaults and the call/caller block (EXP510); `filter_expression` dispatch with aliases and coercion, plus `upper`, `lower`, `capitalize`, `strip`, `replace`, `title`, `lstrip`, `rstrip`, `length`, the array filters `first`, `last`, `join` (EXP512, EXP513) and the serialization/conversion filters `tojson`, `string`, `safe`, `int`, `float` (EXP514) and the `test_expression` dispatcher with its type, identity, numeric, text, equality and membership tests (EXP515), the ordering tests `lt`/`lessthan`/`gt`/`greaterthan`/`ge` and the `startswith`/`endswith` filters (EXP516), `split`/`rsplit` and the `undefined` cell of `truncate` (EXP517), `format` and `indent` (EXP518), the object accessors `keys`/`values`/`items`/`get` and the container filters `sort`/`unique` (EXP519), `map(attribute=)`/`select`/`reject` and `dictsort` (EXP520), and `reverse`/`list`/`min`/`max`/`append`/`pop` with `join(attribute=)` (EXP521) and function values from `range`/`raise_exception` (EXP522) and `dict`/`namespace` (EXP523). Pending: the remaining filters and the rest of the global builtins |
| `common/jinja/value.cpp`, `value.h` | 1 758 + 765 | **Goose (EXP504, EXP505)**: the value model (types, conversions, flags, both equalities, `value_compare`, `value_to_string_repr`) and JSON output (`value_to_json`, `float_repr`, `float_json`). EXP512 ports the string filter built-ins `upper`, `lower`, `capitalize`, `strip`, `replace` and `length` together with the container lengths (`vc`/`vkc`); EXP513 adds `title`, `lstrip`, `rstrip`, the array `first`/`last`/`join`, the undefined-operand table; EXP514 adds the serialization filters `tojson`/`string`/`safe` and the numeric conversions `int`/`float`; EXP515 adds the `test_is_*` predicates for the type/identity/numeric/text/equality/membership families; EXP516 adds the ordering comparisons over `vcompare` and the prefix/suffix filters; EXP517 adds `split`/`rsplit` and the undefined cell of `truncate`; EXP518 adds `format` (simple `{}` placeholders) and `indent` (width/first/blank); EXP519 adds `keys`/`values`/`items`/`get` and `sort`/`unique`. EXP520 adds `map(attribute=)`, `select`/`reject` and `dictsort`, and captures keyword arguments in the filter call form. EXP521 adds `reverse`/`list`/`min`/`max`/`append`/`pop` and the `attribute` form of `join`. EXP522 adds function values: identifiers resolve `range` and `raise_exception`, and macro values can be stored and passed. EXP523 adds `dict`/`namespace` with keyword arguments and object copies. Pending: the remaining per-type tables, the rest of `global_builtins` and the arithmetic operators |
| `common/jinja/string.cpp`, `string.h` | 213 + 61 | **Partial (EXP512, EXP513)**: the byte transforms behind `uppercase`, `lowercase`, `capitalize`, `titlecase` and the two-pointer `strip` (with one-sided trims) are reproduced in Goose. Pending: `string_part` and input marking/origin metadata |
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
6. Runtime: expression evaluation and statement execution — in progress.
   EXP507 ports literals, identifiers, unary/binary operators and membership
   (81 cases, 14/14 mutations, final -O2 gate); EXP508 ports `program`, `if`,
   `set` (block form and tuple unpacking) and the output path (27 cases, 12/12
   mutations); EXP509 ports `for` with its filter and per-iteration scope,
   `break`/`continue` consumption, the loop object and member access (56 cases,
   14/14 mutations, final -O2 gate deferred at the five-minute step limit).
   EXP510 ports the macro definition, calls, positional parameters, defaults and
   the call/caller block (81 cases, 13/13 mutations), with keyword arguments
   bound directly from the call AST instead of the value pool. EXP512 ports the
   filter dispatch, aliases, string coercion and the text filters `upper`,
   `lower`, `capitalize`, `strip`, `replace` plus `length` for strings, arrays,
   tuples and objects (99 cases, 10/10 mutations). EXP513 adds `title`,
   `lstrip`, `rstrip`, the array `first`/`last`/`join`, the undefined-operand
   table and the identifier-form-only coercion rule (122 cases, 12/12
   mutations). EXP514 adds `tojson`, `string`, `safe`, `int` and `float`
   (153 cases, 12/12 mutations). EXP515 adds the `test_expression` node and
   the type/identity/numeric/text/equality/membership tests (187 cases,
   12/12 mutations). EXP516 adds the ordering tests over `vcompare` and the
   `startswith`/`endswith` filters (206 cases, 12/12 mutations). EXP517 adds
   `split`/`rsplit` and the undefined cell of `truncate` (225 cases, 12/12
   mutations). EXP518 adds `format` and `indent` (242 cases, 12/12
   mutations). EXP519 adds `keys`/`values`/`items`/`get` and
   `sort`/`unique` (257 cases, 12/12 mutations). EXP520 adds keyword
   argument capture, `map(attribute=)`, `select`/`reject` and `dictsort`
   (269 cases, 12/12 mutations). EXP521 adds `reverse`/`list`/`min`/`max`/
   `append`/`pop` and `join(attribute=)` (287 cases, 12/12 mutations). EXP522
   adds function values for `range`/`raise_exception` and macro-value
   passing (301 cases, 12/12 mutations). EXP523 adds `dict`/`namespace`
   with keyword arguments (314 cases, 12/12 mutations). Still native: the
   test-name forms of select/reject, map defaults, strftime_now and the
   chat layer.
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
