# EXP473: $ref and rule-name decisions in Goose (P7-09 second slice)

The schema converter reaches definitions through `$ref` and turns every schema
name into a rule name. Both are decisions with visible edge cases: byte-level
sanitization, the fragment split, reserved names, and what happens when two
different schemas sanitize to the same rule name.

## What was ported

`source/refs.goose`, from `_resolve_ref`, `_add_rule`, `visit`,
`is_reserved_name` and `INVALID_RULE_CHARS_RE`:

| decision | rule |
|---|---|
| sanitization | every maximal run of bytes outside `[a-zA-Z0-9-]` becomes one `-` |
| fragment split | everything after the **first** `#`, or the whole ref when there is none |
| ref name | `ref` + sanitized fragment, so `#/definitions/Foo` becomes `ref-definitions-Foo` (the leading `/` collapses into the separator, exactly as native) |
| rule name | reserved → name + `-`; empty → `root`; otherwise unchanged |
| sub-name | name + `-` when the name is not empty |
| collision | keep the sanitized name when absent **or** carrying the same rule text; otherwise append the first free decimal suffix (`name0`, `name1`, ...) |
| resolve | visit the target only when the rule is absent and the ref is not already being resolved; a missing target records `Unresolved $ref` and uses empty rule text |

The reserved set is the native one: `root` plus the twelve primitive rules and
the six string-format rules (19 names). The rule map, rule text and schema walk
stay native.

## Gate

`tests/refs_fixture.cpp` transcribes the native functions
(`std::regex_replace`, `find('#')`, the reserved set, `_add_rule` over a
`std::map`) and compares byte-for-byte.

| corpus | cases |
|---|---|
| sanitize: 19 reserved names, 36 near misses, all 256 single bytes | 305 |
| ref fragment and ref name: fixed cases, the name corpus, 400 random strings | 719 |
| rule name and sub-name over the same corpus | 305 |
| `_add_rule`: 12-step deterministic collision ladder + 900 random insertions over a colliding pool | 912 |
| resolve flags (rule exists x resolving x target) | 8 |
| total | **2 255 cases, 3 291 values, 0 failures** |
| ASan + UBSan over runtime and fixture | 0 failures, rc 0 |

Mutations 14/14 caught: runs not collapsed (314), `_` accepted (205), invalid
byte kept (1189), last `#` (70), `rex` prefix (721), `date-time-string`
unreserved (1), reserved dash dropped (19), no `root` (1), empty sub-name dash
(1), different rule reuses the plain name (659), suffix from 1 (659), dot
separator (659), already-resolving ref revisited (4), missing target visited (4).

## Scope

P7-09 stays PARTIAL. Still native: rule-text emission, `pattern`/`format`
conversion, the schema walk itself and the PEG grammar parser. This slice covers
the name, reference and collision policy the walk depends on.
