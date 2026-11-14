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
| `common/jinja/runtime.cpp`, `runtime.h` | 1 015 + 777 | **Goose (EXP507, EXP508, EXP509)**: expression evaluation and operators (EXP507); `program`, `if`, `set` including the block form and tuple unpacking, the output path and the context binding order (EXP508); `for` with its filter and per-iteration scope, `break`/`continue` consumption, the loop object, member access and tuple-unpack errors (EXP509); `macro` definition, calls, positional parameters, defaults and the call/caller block (EXP510); `filter_expression` dispatch with aliases and coercion, plus `upper`, `lower`, `capitalize`, `strip`, `replace`, `title`, `lstrip`, `rstrip`, `length`, the array filters `first`, `last`, `join` (EXP512, EXP513) and the serialization/conversion filters `tojson`, `string`, `safe`, `int`, `float` (EXP514) and the `test_expression` dispatcher with its type, identity, numeric, text, equality and membership tests (EXP515), the ordering tests `lt`/`lessthan`/`gt`/`greaterthan`/`ge` and the `startswith`/`endswith` filters (EXP516), `split`/`rsplit` and the `undefined` cell of `truncate` (EXP517), `format` and `indent` (EXP518), the object accessors `keys`/`values`/`items`/`get` and the container filters `sort`/`unique` (EXP519), `map(attribute=)`/`select`/`reject` and `dictsort` (EXP520), and `reverse`/`list`/`min`/`max`/`append`/`pop` with `join(attribute=)` (EXP521) and function values from `range`/`raise_exception` (EXP522) and `dict`/`namespace` (EXP523), the `select`/`reject` test forms and `map` defaults (EXP524) and bound method values (EXP525), plus scalar methods with `abs` (EXP526) and the `default` filter (EXP527); EXP528 adds the chat-layer harness that gates the template side against `common_chat_templates_apply`; EXP529 ports the message shape (`role`/`content`); EXP530 ports the caps-driven role workarounds (`developer` mapping, system merge/drop) with the caps flag supplied by a differential probe. EXP531 adds the value usage flag and the system-role caps probe (`supports_system_role` gated against `original_caps()`), so the caps-driven role decision no longer needs a harness-side approximation. EXP532 adds the operation mask and the string/typed content caps flags; EXP533 gates the for-over-string and select-on-string probes (native rejects both; the op mark and the failure flag are what matter); EXP534 adds the tools caps probes (`supports_tools`, `supports_tool_calls`); EXP535 adds the parallel-call and object-argument probes; EXP536 adds the reasoning probes; EXP537 adds the string-content concatenation of typed parts (the typed direction has no public oracle); EXP538 adds the media content shape and the array of parts; EXP539 adds the full message fields (reasoning/name/tool_call_id/tool_calls); EXP540 adds the tool schema serialization and the parameters fallback; EXP541 adds the reverse tools parser with a span-tracking JSON scanner; EXP542 adds escape decoding and the exact tools rejection texts with pretty/compact dumpers; EXP543 tightens the scanner to the RFC 8259 string/number rules and adds contains_media; EXP544 adds common_chat_format_single's tail composition; EXP545 gates two real templates and adds namespace member assignment; EXP546 adds slice/ternary/comment nodes and gates five real templates with zero skips; EXP547 adds object literals and new-key assignment with a rebuild; EXP548 gates fourteen real model templates (Qwen3/Qwen3.5/Qwen3-Coder, GLM-4.6/4.7, LFM2.5, Mistral-Nemo, Ministral-3, Devstral, gemma, deepseek, SmolLM3) plus a tools/thinking dimension, adding selectattr/rejectattr and tojson(indent); EXP549 adds `reasoning_content` to the message normalizer and gates it on the official GLM-5.3 template; EXP550 gates the November-2026 templates (GLM-5.3, LFM3, Qwen3.8-Flash-Next, Mistral-Large-4) and prints whether each template runs on the jinja path or a dedicated builder; EXP551 ports `strftime_now` through a libc shim with an injectable clock; EXP552 ports input marking and the BOS/EOS strip; EXP553 propagates the flag through the identity filters, replace, slice, indent, format and the split tail, matching the native fresh-string rules for join/tojson; pending: the typed direction and the chat API's lack of time injection |
| `common/jinja/value.cpp`, `value.h` | 1 758 + 765 | **Goose (EXP504, EXP505)**: the value model (types, conversions, flags, both equalities, `value_compare`, `value_to_string_repr`) and JSON output (`value_to_json`, `float_repr`, `float_json`). EXP512 ports the string filter built-ins `upper`, `lower`, `capitalize`, `strip`, `replace` and `length` together with the container lengths (`vc`/`vkc`); EXP513 adds `title`, `lstrip`, `rstrip`, the array `first`/`last`/`join`, the undefined-operand table; EXP514 adds the serialization filters `tojson`/`string`/`safe` and the numeric conversions `int`/`float`; EXP515 adds the `test_is_*` predicates for the type/identity/numeric/text/equality/membership families; EXP516 adds the ordering comparisons over `vcompare` and the prefix/suffix filters; EXP517 adds `split`/`rsplit` and the undefined cell of `truncate`; EXP518 adds `format` (simple `{}` placeholders) and `indent` (width/first/blank); EXP519 adds `keys`/`values`/`items`/`get` and `sort`/`unique`. EXP520 adds `map(attribute=)`, `select`/`reject` and `dictsort`, and captures keyword arguments in the filter call form. EXP521 adds `reverse`/`list`/`min`/`max`/`append`/`pop` and the `attribute` form of `join`. EXP522 adds function values: identifiers resolve `range` and `raise_exception`, and macro values can be stored and passed. EXP523 adds `dict`/`namespace` with keyword arguments and object copies; EXP524 adds the select/reject test forms and map defaults through a reusable `dispatch_test`; EXP525 adds bound method values that reuse eval_filter through spare AST nodes; EXP526 adds the scalar method tables and the `abs` filter; EXP527 adds the `default` filter with its boolean check, with objects excluded as natively. Pending: strftime_now, string marking/caps and the arithmetic operators; EXP528 adds the chat-layer harness (native oracle + byte comparison); EXP529 ports the message shape and gates it structurally; EXP530 ports the role workarounds. |
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
   with keyword arguments (314 cases, 12/12 mutations). EXP524 adds the
   select/reject test forms and map defaults (320 cases, 12/12 mutations).
   EXP525 adds bound method values (336 cases, 12/12 mutations). EXP526
   adds scalar methods and `abs` (350 cases, 12/12 mutations). EXP527 adds
   the `default` filter (365 cases, 12/12 mutations). EXP528 adds the
   chat-layer harness and byte comparison against
   `common_chat_templates_apply` (370 cases, 12/12 mutations). EXP529 ports
   the message shape (378 cases, 12/12 mutations). EXP530 adds the
   caps-driven role workarounds (385 cases, 12/12 mutations). Still native:
   the remaining caps probes (EXP531 ports the usage flag and the system-role probe; EXP532 adds the operation mask and the content flags; EXP533 adds the for/select probes; EXP534 adds the tools probes; EXP535 adds the parallel/object-argument probes; EXP536 adds the reasoning probes; EXP537 adds the typed-content concatenation; EXP538 adds the media content shape; EXP539 adds the full message fields; EXP540 adds the tool schemas; EXP541 adds the reverse tools parser; EXP542 adds escape decoding and the tools error texts; EXP543 adds strict JSON validation and contains_media; EXP544 adds format_single's tail composition; EXP545 adds real-template gating and namespace member assignment; EXP546 adds slice/ternary/comment; EXP547 adds object literals and new-key assignment; EXP548 adds modern model templates and the tools/thinking dimension; EXP549 adds reasoning_content; EXP550 adds the 2026 model templates; EXP551 adds strftime_now; EXP552 adds input marking and the BOS/EOS strip; EXP553 adds filter propagation), the typed direction.
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
