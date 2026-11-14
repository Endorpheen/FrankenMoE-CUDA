# EXP557: chat grammar helpers and composition (P7-08 slice 6, part 3)

## Hypothesis

The chat grammar is composed from helpers that sit on top of the builder ported in
EXP556: `tag_with_safe_content`, `optspace`, `prefix`, and the node helpers
`any`, `negate`, `until(marker)` and `rule(name, p)`. If their node shapes are
reproduced exactly, the composed grammar dumps match native byte for byte.

## What the native code does

Read from `common/chat-peg-parser.cpp` and `common/peg-parser.cpp`:

- `tag_with_safe_content(name, marker, p)` with a marker is
  `zero_or_more(choice({ p, rule(name, content(negate(literal(marker)) + any + until(marker))) }))`;
  without a marker the rule body is just `content(any)`;
- `optspace(tag)` starts from `eps()`, then adds one `optional(literal(char))` per
  leading space, the literal body, and one optional per trailing space; because
  `+=` builds a flattened sequence, the dump keeps the leading `Epsilon`;
- `prefix(s, delimiter)` is `eps()` for an empty `s`, `literal(s)` without a
  delimiter, and `literal(s up to the delimiter)` otherwise;
- dump forms: `Any`, `Not(child)`, `Until(marker)` / `Until()`,
  `Rule(name, child)`.

## Port

`peg_any_node`, `peg_not_node`, `peg_until_marker`, `peg_rule_node`,
`peg_optspace`, `peg_prefix` and `peg_tag_safe` are added on the EXP556 builder,
plus the `Sequence`/`Choice` flattening helpers `peg_seq2`, `peg_seq3` and
`peg_choice2` for call sites that build children one by one.

## Compiler finding

An exported function cannot be called both from the C fixture and from Goose
code: the compiler reports `could not resolve one concrete function signature`.
`peg_literal`, `peg_tag`, `peg_seq` and `peg_choice` are therefore split into a
private `_impl` body plus a thin exported wrapper, and internal callers use the
`_impl` variant. The error message names the export declaration, not the call
site, which is why it took a bisect to find.

## Gate

```
FG557_RUNTIME_GRAMMAR cases=662 values=1142 failures=0 skipped=0
mutations 7/7 caught (range 0:7)
mutations 7/7 caught (range 7:14)
baseline after restore: rc=0 cases=662 values=1142 failures=0 skipped=0
```

Sixteen grammars, each built natively and through the port and compared by dump:
`any`, `not`, three `optspace` shapes (plain, spaces around, newline), three
`prefix` shapes (plain, empty, with a delimiter), `tag_with_safe_content` with
and without a marker and with a longer rule name, `tag safe then content`,
`marker before content`, `optspace marker then content`, `two safe tags`, and a
safe tag whose body is a choice.

Fourteen mutations: the until marker, the rule name, the rule's child, the
leading and trailing `optspace` loops, the tag body bounds, the `prefix`
delimiter, an empty prefix, the markerless body, the marker guard, the repetition
bounds, the choice order, the dump of the until marker, and the `Any` text.

## Still open

- The standard JSON/XML tool builders (`standard_json_tools`,
  `standard_constructed_tools`) and `permute`.
- The AST mappers (base, gemma4, minimax) and partial streaming.
- The typed direction, autoparser/grammars, model parsers, `use_jinja=false`, and
  the chat API's missing time injection.
