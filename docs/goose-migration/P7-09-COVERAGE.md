# P7-09 coverage: JSON, schema, PEG and grammar

Status: **PARTIAL**. This document assigns an owner to every function in the five
files named by the criterion, so the remaining work is a list, not a guess.
"Goose" means the decision lives in `experiments/EXP-2026-11-13-*/source/*.goose`
and is gated against the native code quoted in the slice's README. "Boundary"
means the code is the third-party container or an OS primitive; it makes no
conversion decision of its own.

Slices: 472 shaping, 473 `$ref`/names, 474 pattern/format, 475 object text,
476 bounds/const/enum, 477 `_not_strings`, 478 allOf, 479 dispatch/map,
480 GBNF parser, 481 PEG dump, 482-485 PEG engine/AST/invalid, 486-487 PEG
build_grammar, 488-489 Aho-Corasick, 490 add_schema, 491 lazy triggers,
492 JSON-schema builder, 493 JSON parser, 494 JSON dump, 495 schema
introspection, 496 converter output, 497 AST helpers, 498 chars parsing, 499 ref resolution, 500 builder decisions.

## common/json.cpp

| function | owner |
|---|---|
| `common_json::parse`, `parse_no_throw` | Goose (493) — grammar, number classification, escapes, UTF-8, duplicate keys, error classes and positions |
| `common_json::dump`, `dump_safe` | Goose (494) — layout, escaping, integer/unsigned printing, shortest-round-trip doubles, U+FFFD replacement |
| `guard`, `as_json`, `as_common`, `to_json` | Boundary — casting to the nlohmann value |
| `common_json_value` ctors, `common_json` ctors/assign/dtor | Boundary — the document type |
| `is_*`, `empty`, `size`, `contains`, `at`, `operator[]`, `front`, `back`, `clear`, `erase`, `assign`, `set`, `push_back`, `count`, `insert`, `iterator::*`, `items*`, `get<T>`, `operator std::string`, `value` | Boundary — ordered-container accessors; key order is the container's |
| `set_json`/`map_json`/`vec_json` and their `common_json_value` specializations | Boundary — C++ container to document conversion |

`strtod` and `%.*e` are the two C primitives the JSON code calls, both behind a
declared extern (`gs_json_strtod`, `gs_json_fmt_e`).

## common/json-schema.cpp

| function | owner |
|---|---|
| `common_chat_schema_builder::fail`, `get_count`, `get_bound`, `get_format`, `resolve_ref`, `build_ref`, `build_alternatives`, `build_object`, `build_array`, `build_string`, `build_integer`, `build_node`, `build` | Goose (492) |
| `common_chat_schema_from_json` | Goose (492) |
| `json_type` | Goose (495) |
| `value_types_impl`, `common_chat_schema::value_types` | Goose (495) |
| `may_be_string_impl`, `common_chat_schema::may_be_string` | Goose (495) |
| `kind_name`, `type_name` | Goose (495) |

The `common_chat_schema_*` structs themselves are plain data; the PEG builder and
the converter read them through the flat encoding the slices use.

## common/json-schema-to-grammar.cpp

| function | owner |
|---|---|
| `build_repetition` | Goose (474 with an empty separator, 475 with `"," space`) |
| `build_min_max_int`, `digit_range`, `more_digits`, `uniform_range` | Goose (476) |
| `is_reserved_name`, `INVALID_RULE_CHARS_RE`, the reserved set | Goose (473) |
| `GRAMMAR_LITERAL_ESCAPE_RE`, `replacePattern`, `format_literal`, `gbnf_format_literal` | Goose (474) |
| `NON_LITERAL_SET`, `ESCAPED_IN_REGEXPS_BUT_NOT_IN_LITERALS`, `MAX_PATTERN_DEPTH` | Goose (474) — data |
| `gbnf_escape_length` | Goose (474) |
| `_add_rule` | Goose (473, 479) |
| `_generate_union_rule` | Goose (479) |
| `_visit_pattern`, `_pattern_to_rule` | Goose (474) |
| `_not_strings` | Goose (477) |
| `_resolve_ref` | Goose (473) |
| `_add_primitive`, `_visit_primitive` | Goose (479) |
| `add_schema`, the converter constructor, `_visit_all_of`, `visit` | Goose (478, 479) |
| `_generate_constant_rule` | Goose (476) |
| `check_errors` | Goose (496) — the thrown `invalid_argument` text and the warning channel |
| `format_grammar` | Goose (496) — `std::map` order to `name ::= text\n` lines |
| `json_schema_to_grammar` (both overloads), `build_grammar` | Goose for the dispatch (479) and the output (496); the `LLAMA_USE_LLGUIDANCE` branch is not built in this tree |

`GRAMMAR_RANGE_LITERAL_ESCAPE_RE` is the macOS range escape used by
`_pattern_to_rule`'s class path; the port (474) reproduces the escape table
directly, so the regex is not needed. Its equivalence is covered by the 474 gate.

## common/peg-parser.cpp

| function | owner |
|---|---|
| `parser_executor` for literal/sequence/choice/repetition/any/space | Goose (482) |
| `parser_executor` for and/not/atomic/chars/string/until/ac/gbnf | Goose (483) |
| `parser_executor` for rule/tag (node creation) and the node lists | Goose (484) |
| `schema` pass-through, `until` invalid runs, propagation through sequence/rule/tag | Goose (485) |
| `common_peg_ast_arena::add_node` semantics (id order, fields, partial flag) | Goose (484) |
| `common_peg_ast_arena::dump`, `bfs_node` | Goose (484) |
| `common_peg_arena::dump`, `dump_impl` | Goose (481) |
| `common_peg_arena::parse` (entry, result type/positions) | Goose (482) |
| `build_grammar`, `to_gbnf`, `effective_parser`, `collect_reachable_rules`, `schema_delegates` | Goose (486, 487, 490, 491) |
| `gbnf_char_class`, `gbnf_escape_char_class`, `gbnf_ac_grammar`, `gbnf_excluding_grammar`, `gbnf_including_grammar` | Goose (489) |
| Aho-Corasick structure (`common_trie`, fail links, terminals, `next`) | Goose (488) |
| `common_peg_ast_arena::find_by_tag`, `find_by_rule` | Goose (497) — children in order, depth-limited, no self-match |
| `common_peg_ast_arena::visit` (both overloads) | Goose (497) — pre-order DFS from each root |
| `common_peg_ast_node::sanitized_text` | Goose (497) — invalid runs to U+FFFD |
| `common_peg_parse_result_type_name` | Goose (497) — name table |
| `is_hex_digit`, `parse_hex_escape`, `parse_char_class_char`, `parse_char_classes` | Goose (498) — `chars("a-z", …)` pattern to ranges and negation |
| `common_peg_arena::resolve_ref`, `resolve_refs` | Goose (499) — ref children to rule ids, root flattening, unknown-name error |
| `common_peg_arena::add_parser`, `add_rule`, `get_rule` | Boundary — arena storage |
| `rule_name`, `sequence`/`choice` flattening, both `rule()` overloads | Goose (500) — the name cleaner, the same-kind splice and the placeholder/registry policy |
| `literal`, `any`, `space`, `string_content`, `until`, `schema`, `ref`, `atomic`, `tag`, `repeat`, `ac`, `gbnf`, `epsilon`, `start`, `end`, `peek`, `negate`, `number`, `boolean`, `null`, `json_*`, `python_*`, `marker`, `chars` | Boundary — they store their arguments or compose primitives; the trees built through them are gated by 481-491 |

## src/llama-grammar.cpp

| function | owner |
|---|---|
| `is_digit_char`, `is_word_char`, `parse_hex`, `parse_char`, `parse_token` (vocab path, see below), `parse_int`, `parse_space` | Goose (480) |
| `llama_grammar_parser::get_symbol_id`, `generate_symbol_id`, `add_rule`, `parse_alternates`, `parse_sequence`, `parse_rule`, `parse` | Goose (480) |
| `llama_grammar_parser::print` | Boundary — debug printing of the parsed rules |
| `parse_token` (`<...>` tokens) | Boundary — needs a vocabulary; native dereferences a null vocab there, so the corpus cannot reach it |
| `llama_grammar_init_impl`, `llama_grammar_free_impl`, `llama_grammar_accept_impl`, `llama_grammar_apply_impl`, `llama_grammar_*` stacks | Out of P7-09 — the sampling-time matcher; scheduled with the sampling subsystem, not with parsing/rendering |

## Remaining to close P7-09

1. A malformed/deep/large corpus for the grammar and PEG layers, matching the
   JSON corpora of 493/494 (EXP480 already covers 18 malformed grammars; deep and
   large inputs are still missing).

Done: `check_errors` and `format_grammar` (496); the AST helpers `find_by_tag`,
`find_by_rule`, `visit`, `sanitized_text` and `parse_result_type_name` (497);
`chars` pattern parsing (498); `resolve_ref`/`resolve_refs` (499); the builder
decisions (500).
