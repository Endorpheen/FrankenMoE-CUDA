# EXP474: pattern and format conversion in Goose (P7-09 third slice)

The schema converter turns a JSON-schema `pattern` into a GBNF expression and a
string `format` into one of the built-in string rules. This slice ports that
conversion, and — unlike the earlier slices — the gate runs against the **real
native converter**, not a transcription.

## What was ported

`source/pattern.goose`, from `_pattern_to_rule`, `_visit_pattern`,
`gbnf_escape_length`, `build_repetition` and `format_literal`:

| decision | rule |
|---|---|
| anchored check | the pattern must start with `^` and end with `$`; both are stripped |
| literals | consecutive literal bytes are merged into one quoted literal |
| escapes | `\xNN`, `\uNNNN`, `\UNNNNNNNN`, `\t\r\n\\"[]-` are copied; escapes of `^ $ . [ ] ( ) \| { } * + ?` drop the backslash; anything else is unsupported |
| classes | `[...]` is copied verbatim with escape validation; an unterminated class is invalid |
| groups | `(` recurses, `(?:` is accepted, any other `(?` is unsupported; nesting is capped at 100 |
| alternation | `\|` is a spaced rule separator, never merged into a literal |
| repeats | `* + ?` suffix the previous expression; `{n,m}` uses build_repetition with `min-1` semantics and an empty separator |
| sub-rules | a repeated non-literal is interned as `<name>-<n>`; because the native map inserts the entry before reading `size()`, the first index is **1**; a repeated literal is quoted directly |
| wrapping | the result is `"\"" (<rule>) "\""` |
| outcomes | unsupported → fallback to the primitive `string` rule and a warning; invalid → reason recorded and no rule |
| format | `uuid` → uuid rule, `date`/`time`/`date-time` → `*-string`, length bounds → `char{min,max}`, otherwise `string` |
| format_literal | quote a literal, escaping only CR, LF, `"` and `\` (the native regex does not select `-` or `]`) |

The surrounding schema walk, the primitive rule texts and the PEG parser stay
native.

## Gate: the native converter as the oracle

`tests/pattern_fixture.cpp` links the prebuilt `llama-common` objects
(`json-schema-to-grammar.cpp.o`, `json-schema.cpp.o`, `json.cpp.o`, `trie.cpp.o`,
`unicode.cpp.o`) and calls the real `build_grammar`, capturing the warning
channel to classify "unsupported". The Goose result is compared with the native
grammar map: status, the chosen rule name, the exact rule body and, for invalid
patterns, the exception reason.

| corpus | cases |
|---|---|
| 73 fixed patterns × 5 rule-map seed sets (none, `dot`, `root`, `root-1`, all three), plus each with `dotall` | 438 |
| 900 random patterns over the converter's alphabet, random anchors, seeds and `dotall` | 900 |
| `dot` rule text for both `dotall` settings | 2 |
| `string_shape` table (pattern/format/min/max) | 13 |
| `length_rule` against `build_grammar` for min 0..3 × max -1..5 | 24 |
| `format_literal` against `gbnf_format_literal`: empty, all 256 bytes, escapes and 300 random strings | 564 |
| total | **1 941 cases, 4 607 values, 0 failures** |
| ASan + UBSan over the module and fixture | 0 failures, rc 0 |

Seed sets exercise the rule map directly: a seeded `dot` turns the next dot into
`dot0`, a seeded `root` renames the final rule to `root0`, and a seeded `root-1`
pushes the first interned sub-rule to `root-10`.

Mutations 22/22 caught: unanchored accepted (146), merge drops the opening quote
(181), `dots` (24), sub numbering (24), sub separator `X` (24), escapes become
literals (63), class escape copied (14), suffix `#` (62), bounded repetition
loses its upper bound (78), optional becomes `+` (6), trailing backslash (5),
group close leaves the depth (49), `?:` keeps the colon (6), fallback one byte
(739), uuid → date-string (2), length rule spacing (28), group wrapper brackets
(25), quote not escaped (19), alternation as a literal (12), dash escaped (76),
CR raw (87), dotall class (1).

Two mutations initially exposed gaps in the corpus (`{0,1}` and `{1,1}` were
never generated, so the optional-form branch of build_repetition was untested);
those cases were added and the mutation then caught it.

## Reproducing

    bash experiments/EXP-2026-11-13-474-p7-09-pattern/build_fixture.sh

Requires the flint build tree at `llama.cpp-flint-cuda/build-cuda` (the native
objects and `libggml-base.so.0.27.0`). The linked objects are the oracle; the
Goose module is emitted by the pinned compiler into `source/pattern-gen.c`.

## Scope

P7-09 stays PARTIAL. Still native: the schema walk (`visit`, object/tuple/array
rule assembly, integer bounds, `_not_strings`), the built-in rule texts and the
PEG grammar parser.
