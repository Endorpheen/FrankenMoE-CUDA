# P7-09 closure: JSON, JSON-schema, schema-to-GBNF, PEG and grammar

Criterion: "Owned JSON/schema/PEG/grammar Goose; malformed/deep/large grammar
and PEG inputs checked; container and libc primitives only."

Status: **DONE**. Every function of the five files named by the criterion has an
owner (`docs/goose-migration/P7-09-COVERAGE.md`): Goose with a differential gate,
or a documented boundary that makes no conversion decision. The malformed, deep
and large corpus is EXP501.

## Slices

Thirty atomic slices, `EXP-2026-11-13-472` … `501`. Each one carries a
differential gate against the native object from `llama.cpp-flint-cuda`, an
ASan+UBSan run and a mutation campaign.

| slice | surface | gate (cases / values) | mutations |
|---|---|---|---|
| 472 | schema shaping: kind dispatch, object/array/tuple, `additionalProperties` | 369 / 1 569 | 8/8 |
| 473 | `$ref` resolution, rule names, reserved names | 2 255 / 3 291 | 14/14 |
| 474 | pattern to GBNF, formats, literal escaping | 1 941 / 4 607 | 22/22 |
| 475 | converter visit: object text, member separation | 49 / 113 | 18/18 |
| 476 | integer bounds, const and enum rules | 203 / 203 | 17/17 |
| 477 | `_not_strings` exclusion | 76 / 76 | 14/14 |
| 478 | `allOf` | 24 / 25 | 16/16 |
| 479 | `_visit_primitive`, dispatch, rule map | 21 / 21 | 15/15 |
| 480 | GBNF rule-text parser | 46 / 322 | 18/18 |
| 481 | PEG `dump` | 13 / 13 | 14/14 |
| 482 | PEG executor, first half | 2 130 / 4 260 | 15/15 |
| 483 | PEG executor, second half (until, chars, string, ac, gbnf…) | 2 080 / 2 080 | 16/16 |
| 484 | PEG AST nodes, ids, fields, dump | 368 / 3 986 | 14/14 |
| 485 | invalid UTF-8 runs and propagation | 512 / 3 081 | 11/11 |
| 486 | `build_grammar` rule emission | 21 / 21 | 14/14 |
| 487 | `build_grammar`: chars, string classes, schema delegation | 29 / 29 | 9/9 |
| 488 | Aho-Corasick automaton | 12 / 987 | 4/4 |
| 489 | AC grammar emission (`gbnf_ac_grammar`) | 9 / 18 | 8/8 |
| 490 | non-delegating schemas (`add_schema`) | 31 / 31 | 5/5 |
| 491 | lazy triggers in `build_grammar` | 35 / 35 | 5/5 |
| 492 | JSON-schema document builder | 85 / 541 | 23/23 |
| 493 | JSON text parser | 78 / 81 850 | 21/21 |
| 494 | JSON `dump` / `dump_safe` | 188 / 188 | 23/23 |
| 495 | schema introspection (`value_types`, `may_be_string`, names) | 56 / 163 | 15/15 |
| 496 | converter output (`format_grammar`, `check_errors`) | 24 / 34 | 12/12 |
| 497 | PEG AST helpers (`find_by_*`, `visit`, `sanitized_text`) | 55 / 3 085 | 13/13 |
| 498 | PEG `chars` pattern parsing | 42 / 158 | 13/13 |
| 499 | PEG `resolve_ref` / `resolve_refs` | 24 / 271 | 12/12 |
| 500 | builder decisions (`rule_name`, flattening, `rule()`) | 20 / 64 | 10/10 |
| 501 | malformed / deep / large corpus | 49 / 190 379 | 7/7 |
| **total** | | **10 845 / 301 501** | **406/406** |

Every slice: 0 failures, ASan+UBSan clean.

## Malformed, deep and large inputs

EXP501 covers the third clause of the criterion explicitly.

| class | evidence |
|---|---|
| malformed grammar | EXP480 (18 malformed grammars) and EXP501 (unterminated 100 000-byte literal, 300-byte name missing `::=`, `{2001}` ceiling), exact native error text |
| deep grammar | EXP501: 500 nested groups, 200 nested `{2}`, 1 500 repetitions of a doubly grouped literal |
| large grammar | EXP501: 20 000 rules, 100 000-byte literal, 10 000 alternatives; full symbol map and every element compared |
| malformed PEG input | EXP485 (invalid UTF-8 runs, 512 cases) |
| deep PEG | EXP501: 600 nested repetitions |
| large PEG | EXP501: 200 000-byte inputs through repetition, literal, `until` with 200 delimiters, `chars`, string content and `ac` |

## Defect the corpus found

`handle_reps` copied the repeated expression into two local `i64[1024]` arrays.
A 4 000-byte literal with `{2}` overflowed that scratch and was rejected while
native accepted it. EXP501 moves the copy into the tail of the caller-sized rule
arena and adds an explicit guard with its own message. The fixed module is kept
in EXP501; EXP480's card is unchanged.

## Boundary (allowed by the criterion)

| boundary | why it makes no conversion decision |
|---|---|
| nlohmann `ordered_json` as the `common_json` backing value | the container: storage, iterators, member order; parse and dump are Goose (493/494) |
| `strtod` (`gs_json_strtod`) | correctly rounded decimal to double, a libc primitive with a declared extern |
| `%.*e` (`gs_json_fmt_e`) | correctly rounded digit extraction for `dump`; the shortest-precision search and layout are Goose |
| C++ container constructors (`to_json`, `set_json`, `map_json`, `vec_json`) | cast a C++ value into the document type |
| `llama_grammar_parser::print` | debug printing of already-parsed rules |
| `parse_token` (`<...>`) | needs a vocabulary; native dereferences a null vocab, so the corpus cannot reach it |
| `llama_grammar_init/accept/apply` and the stacks | sampling-time matcher, scheduled with the sampling subsystem, not with parsing/rendering |

## Verdict

P7-09 closes. The owned JSON, schema, schema-to-GBNF, PEG and GBNF-parse surface
is Goose with differential gates, mutation evidence and clean sanitizers; the
malformed/deep/large matrix is EXP501; the remaining native code is the
third-party JSON container and the two libc number primitives the criterion
allows.
