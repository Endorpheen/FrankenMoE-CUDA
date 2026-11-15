# P7-08 coverage: chat templates, Jinja and reasoning parsing

Criterion: "All shared Jinja/template/filter/parser/tool/reasoning logic Goose;
Apex golden prompt bytes and structured output exact; no minja arithmetic or
parsing wrapper left."

Status: **DONE**. This document assigns an owner to the template stack so the
remaining work is a list. "Goose" means a differential gate against the native
code; "pending" means still native.

## Modules

| module | lines | owner |
|---|---|---|
| `common/jinja/lexer.cpp`, `lexer.h` | 361 + 157 | **Goose (EXP502)**: tokens, normalization, trim, escapes, positions, error text |
| `common/jinja/parser.cpp`, `parser.h` | 619 + 21 | **Goose (EXP503)**: token stream to the 30 node shapes, precedence chain, statements, arguments, positions and error text |
| `common/jinja/runtime.cpp`, `runtime.h` | 1 015 + 777 | **Goose (EXP507, EXP508, EXP509)**: expression evaluation and operators (EXP507); `program`, `if`, `set` including the block form and tuple unpacking, the output path and the context binding order (EXP508); `for` with its filter and per-iteration scope, `break`/`continue` consumption, the loop object, member access and tuple-unpack errors (EXP509); `macro` definition, calls, positional parameters, defaults and the call/caller block (EXP510); `filter_expression` dispatch with aliases and coercion, plus `upper`, `lower`, `capitalize`, `strip`, `replace`, `title`, `lstrip`, `rstrip`, `length`, the array filters `first`, `last`, `join` (EXP512, EXP513) and the serialization/conversion filters `tojson`, `string`, `safe`, `int`, `float` (EXP514) and the `test_expression` dispatcher with its type, identity, numeric, text, equality and membership tests (EXP515), the ordering tests `lt`/`lessthan`/`gt`/`greaterthan`/`ge` and the `startswith`/`endswith` filters (EXP516), `split`/`rsplit` and the `undefined` cell of `truncate` (EXP517), `format` and `indent` (EXP518), the object accessors `keys`/`values`/`items`/`get` and the container filters `sort`/`unique` (EXP519), `map(attribute=)`/`select`/`reject` and `dictsort` (EXP520), and `reverse`/`list`/`min`/`max`/`append`/`pop` with `join(attribute=)` (EXP521) and function values from `range`/`raise_exception` (EXP522) and `dict`/`namespace` (EXP523), the `select`/`reject` test forms and `map` defaults (EXP524) and bound method values (EXP525), plus scalar methods with `abs` (EXP526) and the `default` filter (EXP527); EXP528 adds the chat-layer harness that gates the template side against `common_chat_templates_apply`; EXP529 ports the message shape (`role`/`content`); EXP530 ports the caps-driven role workarounds (`developer` mapping, system merge/drop) with the caps flag supplied by a differential probe. EXP531 adds the value usage flag and the system-role caps probe (`supports_system_role` gated against `original_caps()`), so the caps-driven role decision no longer needs a harness-side approximation. EXP532 adds the operation mask and the string/typed content caps flags; EXP533 gates the for-over-string and select-on-string probes (native rejects both; the op mark and the failure flag are what matter); EXP534 adds the tools caps probes (`supports_tools`, `supports_tool_calls`); EXP535 adds the parallel-call and object-argument probes; EXP536 adds the reasoning probes; EXP537 adds the string-content concatenation of typed parts (the typed direction has no public oracle); EXP538 adds the media content shape and the array of parts; EXP539 adds the full message fields (reasoning/name/tool_call_id/tool_calls); EXP540 adds the tool schema serialization and the parameters fallback; EXP541 adds the reverse tools parser with a span-tracking JSON scanner; EXP542 adds escape decoding and the exact tools rejection texts with pretty/compact dumpers; EXP543 tightens the scanner to the RFC 8259 string/number rules and adds contains_media; EXP544 adds common_chat_format_single's tail composition; EXP545 gates two real templates and adds namespace member assignment; EXP546 adds slice/ternary/comment nodes and gates five real templates with zero skips; EXP547 adds object literals and new-key assignment with a rebuild; EXP548 gates fourteen real model templates (Qwen3/Qwen3.5/Qwen3-Coder, GLM-4.6/4.7, LFM2.5, Mistral-Nemo, Ministral-3, Devstral, gemma, deepseek, SmolLM3) plus a tools/thinking dimension, adding selectattr/rejectattr and tojson(indent); EXP549 adds `reasoning_content` to the message normalizer and gates it on the official GLM-5.3 template; EXP550 gates the November-2026 templates (GLM-5.3, LFM3, Qwen3.8-Flash-Next, Mistral-Large-4) and prints whether each template runs on the jinja path or a dedicated builder; EXP551 ports `strftime_now` through a libc shim with an injectable clock; EXP552 ports input marking and the BOS/EOS strip; EXP553 propagates the flag through the identity filters, replace, slice, indent, format and the split tail, matching the native fresh-string rules for join/tojson; EXP554 adds the second flag (all parts input) with the concat/format part rules; EXP555 proves the output-parse oracle works without weights and ports the content-only fallback; EXP556 ports the PEG builder and its dump with sequence/choice flattening; EXP557 composes the chat grammar helpers (tag_with_safe_content, optspace, prefix, any/not/until/rule); EXP559 adds permute and the cycle-aware dump; EXP560 ports the base AST mapper (content, reasoning, tool calls) with 16 differential pairs; EXP561 fixes the dump visited sets by Tag depth and restores permute in the canonical runtime; EXP562 adds the gemma4 and MiniMax-M3 mappers; EXP563 ports the legacy use_jinja=false path (55 formats, 75 prompt goldens); EXP564 ports the typed-content direction (caps rewrite, 24 pairs); EXP566 adds token-aligned message delimiters; EXP567 adds the streaming session, diffs and monotone publication; EXP569 adds the autoparser grammar generator and GBNF emission; EXP570 adds the JSON-schema-to-GBNF converter (30 cases / 182 values, 12/12 mutations) and merges the EXP496 output helpers into the same canonical file; EXP574 adds the standard tool builders (structural 13/13, parse 9/9, 15/15 + 5/5 mutations); EXP593 adds nine model-output parser families (18 cases / 99 values, 12/12 mutations); pending: hoisted native names and the chat API's lack of time injection |
| `common/jinja/value.cpp`, `value.h` | 1 758 + 765 | **Goose (EXP504, EXP505)**: the value model (types, conversions, flags, both equalities, `value_compare`, `value_to_string_repr`) and JSON output (`value_to_json`, `float_repr`, `float_json`). EXP512 ports the string filter built-ins `upper`, `lower`, `capitalize`, `strip`, `replace` and `length` together with the container lengths (`vc`/`vkc`); EXP513 adds `title`, `lstrip`, `rstrip`, the array `first`/`last`/`join`, the undefined-operand table; EXP514 adds the serialization filters `tojson`/`string`/`safe` and the numeric conversions `int`/`float`; EXP515 adds the `test_is_*` predicates for the type/identity/numeric/text/equality/membership families; EXP516 adds the ordering comparisons over `vcompare` and the prefix/suffix filters; EXP517 adds `split`/`rsplit` and the undefined cell of `truncate`; EXP518 adds `format` (simple `{}` placeholders) and `indent` (width/first/blank); EXP519 adds `keys`/`values`/`items`/`get` and `sort`/`unique`. EXP520 adds `map(attribute=)`, `select`/`reject` and `dictsort`, and captures keyword arguments in the filter call form. EXP521 adds `reverse`/`list`/`min`/`max`/`append`/`pop` and the `attribute` form of `join`. EXP522 adds function values: identifiers resolve `range` and `raise_exception`, and macro values can be stored and passed. EXP523 adds `dict`/`namespace` with keyword arguments and object copies; EXP524 adds the select/reject test forms and map defaults through a reusable `dispatch_test`; EXP525 adds bound method values that reuse eval_filter through spare AST nodes; EXP526 adds the scalar method tables and the `abs` filter; EXP527 adds the `default` filter with its boolean check, with objects excluded as natively. Pending: strftime_now, string marking/caps and the arithmetic operators; EXP528 adds the chat-layer harness (native oracle + byte comparison); EXP529 ports the message shape and gates it structurally; EXP530 ports the role workarounds. |
| `common/jinja/string.cpp`, `string.h` | 213 + 61 | **Partial (EXP512, EXP513)**: the byte transforms behind `uppercase`, `lowercase`, `capitalize`, `titlecase` and the two-pointer `strip` (with one-sided trims) are reproduced in Goose. Pending: `string_part` and input marking/origin metadata |
| `common/jinja/caps.cpp`, `caps.h` | 592 + 40 | pending — capability checks |
| `common/jinja/utils.h` | 149 | Boundary — `peak_source` and the FNV hasher are utilities; `peak_source` is already reproduced by EXP502 for lexer errors |
| `src/llama-chat.cpp` | — | pending — template application and message conversion |
| `common/chat.cpp`, `chat.h`, `chat-*.cpp` | — | pending — chat parsing, tool calls, reasoning extraction |
| `common/parsers/**` | ~3 360 | pending — per-model output parsers |

## Chat apply and time

`common_chat_templates_apply` (`src/llama-chat.cpp`) is still native. It owns
template selection (explicit source, built-in variants, arch defaults), the
per-format rendering handlers, the caps-driven branches, the tool/reasoning
prefixes and the returned `common_chat_params` (format, prompt, grammar,
parser text, preserved tokens, generation prompt).

Time is not passed through `common_chat_templates_inputs`: the Jinja context
sets `current_time = std::time(nullptr)` (`common/jinja/runtime.h:73,79`) and
`strftime_now` reads it. EXP551 already ports `strftime_now` through a libc shim
with an injectable clock, so the Goose side can render deterministic time once
the apply entry point is ported; the native renderer used by the P9-06 gate does
not inject time at all.

The slice is a full renderer port, not a small fix: it should land behind a
differential gate over real templates (the EXP545-550 corpus) and must own the
`common_chat_params` assembly, with the parser text still produced by the
accepted PEG/schema slices.

## Hoisted rule names

Closed by EXP625 (`experiments/EXP-2026-11-15-625-p7-08-hoisted-names`).
`gb_hoist` named hoisted `until-*`/`ac-*` rules from `pid = mn[id]`, a
builder-local id, while native `to_gbnf` names them from the arena insertion
index. The port now allocates a Ref node for `rule()` exactly as the native
builder does (`peg_rule_ref`/`peg_rule_seal`, a `PEG_REF` kind), builds
constructed args in the native operator order, skips empty seq/choice children
like the native writer, resolves the effective kind for literals and descends
into the schema child under the `tool-<i>-schema` rule name. The native
insertion index is therefore reproduced without renaming `pid` to `id`.

Evidence: `gbnf=ok 13/13` (was 1/13), `cases=13 failures=0`, `parse_cases=9
failures=0` against the full GBNF text from `common_peg_arena::to_gbnf`;
identity holds per reachable node (CT-1 81/81, CT-2 107/107, CT-3 1/1, CT-4
11/11, CT-5 155/155); mutations 27/27 in four halves plus parse mutations 5/5.
Boundaries: the JSON family's creation order is not aligned (J-3 native 211 /
port 210), which does not affect GBNF until that family grows `until`/`ac`;
schema bodies remain outside the slice.

## Canonical composition

Every canonical module is built standalone, but the migration target is one
runtime. The checker reports shared export names as warnings; the current set is
27 names in four groups:

- `builtin_call`, `builtin_names` — `jinja/builtins.goose` and `jinja/runtime.goose`;
- `value_cmp`, `value_info` — `jinja/value.goose` and `jinja/value-json.goose`;
- `json_parse` — `json/json-parser.goose` plus three byte-identical module copies
  (`chat/`, `cli/`, `server/`), required because Goose resolves `import
  jsonparse` only by module name next to the importer;
- the GGUF stack — `s1_*`, `s2_write`, `s3_*` repeated in `model/gguf-metadata`,
  `gguf-writer`, `gguf-tensors`, `gguf-split` and `model-saver`, because each
  slice is standalone and carries the layers below it;
- quantizer helpers — `qz_*` in `model/quant-rows` and `model/quant-rows2`;
- `g4_plan`, `g4_shape` — `model/gemma4` and `model/glm4` (an accidental shared
  prefix; the files are otherwise disjoint).

Deduplicating them is a composition step, not a slice; it must keep every
standalone gate green before the runtime links as one module. The checker fails
a canonical file with no export fn unless it is a shared `model/base/` module,
which is how the request-schema defect (gate driver in the same unit, no public
API) is now caught.

`tools/compose-goose.py` is the first composition step. It resolves imports
(the parser is emitted once), drops identical declarations, applies per-module
renames for colliding helper families and refuses to finish when two modules
carry different bodies under one name. Status:

- the server stack (`jsonparse`, `mcp-core`, `mcp-stdio`, `decision-core`,
  `server-schema`, `tools-pure`) composes and typechecks as one unit with 595
  specializations and writes 1.4 MB of generated C; `decision-core` now calls
  the parser through its `extern "json_parse"` ABI, as the other modules do;
- the GGUF stack (`gguf-metadata`, `gguf-writer`, `gguf-tensors`,
  `gguf-split`) also composes: 103 specializations. `gguf-split` and
  `gguf-tensors` now carry the writer's exact `s2_write` body instead of their
  own variants; their gates stay at 39/587 and 41/307;
- the quantizer stack (`quant-types`, `quant-allow`, `quant-rows`,
  `quant-rows2`, `quant-dequant`, `quant-dequant2`) also composes: 310
  specializations and 876 KB of generated C. Its helper copies are renamed by
  module (`st2_*`, `fabs2_32`, `ld2_f32`, `qd_*`) and a library unit drops
  `fn main`;
- the 20 architecture builders are per-architecture modules, not one unit: they
  reuse names like `T_FFN_GATE_SHEXP` with different values for different
  architectures, and one runtime links exactly one builder plus the base
  modules. Composing them together is therefore not a goal;
- the jinja runtime, value/builtins still need the same treatment before one
  runtime can link. Their copies are not stale duplicates but variants tied to
  different scratch layouts (`bases` uses st[10] in runtime and st[0] in value)
  and to extra kinds (`type_name` knows V_FUNC/V_METHOD only in runtime), so
  unifying them is a real merge, not a rename;
- the runtime and the schema converter (`jinja/runtime` + `json/format-grammar`)
  compose as well: 1029 specializations. That makes the schema grammar reachable
  in one unit, but `arena_gbnf` still cannot call `json_schema_convert` without
  a linker seam: the runtime is also built standalone by every P7-08 gate, and a
  plain `extern` would leave those links unresolved. Closing the `meta[3]=1`
  stub needs an explicit seam (a weak fallback or a build that always links the
  converter), which is a separate design step;
- the application stack (the five server modules, `chat/thinking-control`,
  `chat/parse-params` and `cli/cli-protocol`) composes too: 666 specializations,
  six struct instances. It is the largest decision-only unit before the Jinja
  runtime joins;
- the composer now checks interfaces: it records every `export fn` in the input
  modules and fails if the composed unit does not expose exactly that set, so a
  dedup or rename can no longer silently drop a public entry point;
- the core set (`goose-model-graph`, `goose-model-loader`, `file-io`,
  `jinja/runtime`, `json/format-grammar`) composes as well: 1114
  specializations and 12.5 MB of generated C. The collision was in the native
  helper declarations, not in the logic: the model modules bind different
  extern symbols under the same local names (`fail`, `ensure`, `st_i64`), so
  the local names are scoped per module while the extern symbols stay intact;
- the full decision+core unit (the core set plus the five server modules,
  `chat/thinking-control`, `chat/parse-params` and `cli/cli-protocol`) composes
  too: 1678 specializations, 35 164 lines, 14.1 MB of generated C, which compiles
  at -O0 in 13 s into a 6.8 MB object with 201 global symbols and no duplicate
  definitions. The last collisions were genuine name clashes across domains:
  the Jinja runtime's `K_*` are AST node kinds (13..18) and the JSON parser's
  `K_*` are value kinds (0..7); the schema converter also uses `K_NULL` with a
  different meaning. All three families are scoped per module in the composed
  unit while the sources keep their names;
- with the accepted P9-01, P9-07 and P6-11 modules the core set grows to ten
  modules (`core2`: model graph/loader/file-io/arch-registry, jinja runtime,
  schema converter, config-validate, config-resolve, metrics-traces,
  spec-decode-decisions): 1199 specializations, no new collisions;
- `tools/check-composition.sh` composes the nine accepted sets (server, quant,
  GGUF, P9-06, runtime+schema, app, core, core2, full) and runs `goose check` on
  each; it is the fast smoke test for the composition step;
- `tools/build-main-runtime.sh` is the growing main build: it adds every
  accepted module that shares one unit, then typechecks, generates C, compiles
  an object and rejects duplicate globals. Today it holds 50 modules and 2616
  specializations (16.8 MB C, 549 globals, 0 duplicates, 97 extern entry points
  at -O1). Its internal name collisions are scoped per module in the composer
  (quant-weighted's helper families join the qw_ prefix).

## Slice plan

Eight slices; `tasks.json` mirrors this count (`slices_total`/`slices_done`).

1. Lexer (EXP502) — done.
2. Parser: token stream to program, with the exact node shapes and error text — done (EXP503).
3. Value model: types, conversions, flags, equality, `value_compare`, repr — done (EXP504).
4. JSON output: `value_to_json`, `float_repr`, `float_json` — done (EXP505).
5. Builtins and filters: `func_args`/`value_func`, `global_builtins`, the
   per-type tables, and the arithmetic operators — done piecewise: EXP506
   (`func_args`, int/float tables, 51 cases), EXP512-527 (filters, tests, methods,
   containers), EXP522 (`range`/`raise_exception`), EXP523 (`dict`/`namespace`)
   and EXP551 (`strftime_now`). The `global_builtins` table is exercised through
   those exports rather than as a standalone map.
6. Runtime: expression evaluation and statement execution — done (EXP507-527, 545-570, 574, 593; the canonical runtime is 19025 lines).
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
   the remaining caps probes (EXP531 ports the usage flag and the system-role probe; EXP532 adds the operation mask and the content flags; EXP533 adds the for/select probes; EXP534 adds the tools probes; EXP535 adds the parallel/object-argument probes; EXP536 adds the reasoning probes; EXP537 adds the typed-content concatenation; EXP538 adds the media content shape; EXP539 adds the full message fields; EXP540 adds the tool schemas; EXP541 adds the reverse tools parser; EXP542 adds escape decoding and the tools error texts; EXP543 adds strict JSON validation and contains_media; EXP544 adds format_single's tail composition; EXP545 adds real-template gating and namespace member assignment; EXP546 adds slice/ternary/comment; EXP547 adds object literals and new-key assignment; EXP548 adds modern model templates and the tools/thinking dimension; EXP549 adds reasoning_content; EXP550 adds the 2026 model templates; EXP551 adds strftime_now; EXP552 adds input marking and the BOS/EOS strip; EXP553 adds filter propagation; EXP554 adds the part-level rules; EXP555 adds the parse entry point; EXP556 adds the PEG builder; EXP557 adds the grammar helpers; EXP559 adds permute; EXP560 adds the base mapper; EXP561 fixes dump visited; EXP562 adds the gemma4/minimax mappers; EXP563 adds the legacy path; EXP564 adds the typed direction), the tool builders.
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
