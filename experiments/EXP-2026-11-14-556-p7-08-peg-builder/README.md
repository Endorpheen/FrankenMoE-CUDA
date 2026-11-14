# EXP556: PEG builder and grammar dump (P7-08 slice 6, part 2)

## Hypothesis

`common_chat_peg_builder` is a thin layer over the already ported PEG node model:
tag helpers (`content`, `reasoning`, `reasoning_block`, `tool_open`, `tool_name`,
`tool_close`, ...) plus `atomic`. If the node shape is the same, a ported builder
can be gated against `common_peg_arena::dump()` byte for byte, without the AST
mapper.

The probe confirmed the dump is stable and readable, for example
`Sequence(Tag(content, Until()), End)` for the static content-only grammar.

## What the native builder does

Read from `peg-parser.cpp` and `chat-peg-parser.h`:

- nodes are appended to the arena, so a build's root is the last node;
- a parent stores one contiguous child run;
- `sequence()` and `choice()` **flatten** a nested node of the same kind into the
  parent, so `sequence({ a, sequence({ b, c }) })` dumps as one `Sequence(a, b, c)`;
- tag helpers wrap their child in `Tag(name, child)`; `tool_open`, `tool_close`,
  `tool_id`, `tool_name` also wrap the tag in `Atomic`;
- dump formats: `Epsilon`, `End`, `Until()`, `Literal(text)`, `Tag(name, child)`,
  `Atomic(child)`, `Sequence(a, b)`, `Choice(a, b)`,
  `Repetition(child, min, unbounded)` with `unbounded` for a negative max.

## Port

`peg_rest`/`peg_end_node`/`peg_eps_node`/`peg_literal`/`peg_tag`/`peg_atomic`/
`peg_seq`/`peg_choice`/`peg_repeat` manage a node arena (kind, child range,
string index, min/max, string pool) and `peg_dump_basic` reproduces the native
textual form for those kinds. Sequence and choice flatten like the native builder.

Node kinds outside this slice (chars, string, until-with-delimiters, schema, rule,
ref, gbnf, ac) and the standard JSON/XML tool builders stay native.

## Gate

```
FG556_RUNTIME_PEG cases=646 values=1126 failures=0 skipped=0
mutations 7/7 caught (range 0:7)
mutations 7/7 caught (range 7:14)
baseline after restore: rc=0 cases=646 values=1126 failures=0 skipped=0
```

Eighteen grammars, each built twice (native lambda and port calls) and compared by
dump: content+end, reasoning+end, reasoning block, literal+end, content choice,
sequence of three, atomic tool-open, tool-close of a tool-name, one-or-more,
optional, bounded repetition, rest alone, nested tags, choice of three, a literal
with spaces and quotes, epsilon, nested sequences, and flattening after a tagged
child.

Fourteen mutations: string length, pool cursor, string index, node count, child
range start, child count, the tag's child, repetition bounds, atomic-as-tag,
sequence-as-choice, disabled flattening, flattened range, tag punctuation, and a
repetition printing its minimum.

## Findings recorded

- Flattening is observable and was missing from the first port attempt; the gate
  caught it with the nested-sequence case.
- `Atomic` is not cosmetic: `tool_open` and `tool_close` add it, which is why the
  tool cases build `Atomic(Tag(...))`.
- One mutation (node count) is detected by a stack overflow instead of a value
  mismatch, because the dump follows a bogus root; the harness reports that
  distinction. This port's dump has no visited set, unlike native `[cycle]`
  handling, because the builder never shares nodes.

## Still open

- The chat grammar composition (content/reasoning/tool sections with markers and
  permutations) and `standard_json_tools`/`standard_constructed_tools`.
- The AST mappers and partial streaming.
- The typed direction, autoparser/grammars, model parsers, `use_jinja=false`, and
  the chat API's missing time injection.
