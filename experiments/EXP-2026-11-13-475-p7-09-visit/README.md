# EXP475: visit() rule-text assembly in Goose (P7-09 fourth slice)

Objects, tuples, arrays and unions are where the converter spends most of its
text: the required chain, the optional alternatives with their recursive
`-rest` rules, the additional-properties branch and the repetition forms with a
separator. This slice ports that assembly and gates it against the real
`build_grammar` oracle on constructed schemas.

## What was ported

`source/visit.goose`, from `_build_object_rule`, the tuple/array/anyOf branches
of `visit`, `build_repetition` with a separator and the kv rules:

| decision | rule |
|---|---|
| object body | `"{" space ` + required kv chain + optional block + ` space "}"` |
| required chain | kv rules separated by ` "," space ` |
| optional block | ` (` then ` "," space ( ` when required properties exist, one alternative per optional property, `)?` |
| recursive rest | an optional at position i with more optionals after it adds `<name>-<key>-rest` holding the chain from i+1, and the branch references it; identical name/text reuses the rule |
| `*` branch | first: `( "," space <kv> )*`; later: `<kv> ( "," space <kv> )*`; never `<kv>?` |
| empty object | `"{" space "}"` |
| tuple | `"[" space ` + items joined by ` "," space ` + ` space "]"` |
| array | `"[" space ` + build_repetition(item, min, max, `"," space`) + ` space "]"`, negative max becomes unbounded |
| anyOf | alternative rule names joined by ` | ` |
| kv | `format_literal(dump) + ' space ":" space ' + child` |
| additional kv | `key_rule + ' ":" space ' + value_rule` |
| object branch | empty object + ANY additional maps to the `object` primitive instead of a build |

Three native behaviours were found while writing the port and are reproduced
exactly:

1. `build_grammar` calls `add_schema` with an **empty name** for root, so the
   kv rules are `a-kv` and the rest rules are `b-rest`, not `root-*`.
2. `build_repetition` drops the separator in its recursive call, so the inner
   repetition always takes the empty-separator branch: an unbounded array is
   `(string ("," space string)*)?`, not an infinite recursion.
3. With no required properties but a typed additional-properties entry the body
   contains two spaces before the optional block (`space  (`), because the
   optional block starts with its own space.

## Gate

`tests/visit_fixture.cpp` builds real `common_chat_schema_*` trees, runs the
native `build_grammar` and compares the root body, each property kv rule, the
additional kv rule and every emitted `-rest` rule (name and text). Child value
rule names are the primitive names the native visit returns.

| corpus | cases |
|---|---|
| objects: empty, empty+ANY, empty+typed, one/all required, 2-3 optionals, typed and ANY additional, weird and unicode names | 14 |
| tuples of 0-3 primitive items | 4 |
| arrays: min 0..3 x max -1..5 | 28 |
| anyOf of 1-3 primitives | 3 |
| total | **49 cases, 113 values, 0 failures** |
| ASan + UBSan over module and fixture | 0 failures, rc 0 |

Mutations 18/18 caught: required comma (1), optional opener (10), required-optional
comma (7), star as `?` (5 and 3), rest separator `X` (14), rest dedupe (2), kv
link space (8), empty object (1), tuple comma (2), array brackets (4), negative
max (4), `+`→`*` (1), `*`→`+` (2), union comma (2), kv quotes (24), reversed
optionals (10), empty-object check (2).

## Scope

The fixture keeps property names distinct after sanitization, so native collision
suffixes on kv names are not exercised here (that policy is EXP473).

P7-09 stays PARTIAL. Still native: the visit dispatch and recursion, `$ref`
targets, `_not_strings` (the trie text for typed additional properties),
integer bounds (`build_min_max_int`), const/enum JSON dumping, allOf and the PEG
parser.
