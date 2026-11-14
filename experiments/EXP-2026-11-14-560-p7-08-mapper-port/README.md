# EXP560: base AST mapper port (P7-08 slice 7, part 1)

## Hypothesis

The base `common_chat_peg_mapper` is a single preorder walk whose state lives for
one call. A flat port over the existing node model, with the same buffers and the
same branch order, reproduces the native message field by field on hand-built
ASTs, without a parser and without weights.

## Starting point and review

The first draft was produced by a helper agent. Before it was taken as a base it
was rebuilt and re-run independently here: `goose check` clean,
`goose --standalone` builds, and the helper's 12 differential pairs report
`RESULT cases=12 failures=0`. The draft was then merged into the canonical
runtime next to the existing Jinja and PEG slices, and its own review notes were
kept as the limitation list below.

## What the native mapper does

- One preorder walk: a node is dispatched by its tag, then its children in order,
  so a tagged node nested in a tagged node contributes its text again.
- `content` and `reasoning` append `sanitized_text()` with no separator and no
  trim; reasoning that ends up whitespace-only is dropped after the walk.
- `trim()` removes at most one leading space and every trailing space.
- `tool-open` starts a fresh pending call; `tool-id` trims and strips one pair of
  surrounding quotes; `tool-name` flushes `args_buffer` (or `"{"` when empty) and
  pushes the call immediately, so later argument writes go to the pushed entry.
- `arg-name` writes `,` (except first) plus a JSON-quoted key and `:`;
  `arg-value` appends `normalize_quotes_to_json`; a schema-declared string value
  appends its escaped inner text inside one pair of quotes closed by `arg-close`.
- `tool-close` closes a pending quote and every unclosed `{` by brace depth and
  pushes only a named pending call.

## Gate

The fixture builds the same grammar on both sides, parses with the ported
`common_peg_arena`, runs the native mapper and the ported one, and compares role,
content, reasoning, call count and every name/arguments/id.

```
FG560_RUNTIME_MAPPER cases=16 values=87 failures=0 skipped=0
```

Sixteen pairs: content only, reasoning before content, two content nodes,
XML-constructed tools with two arguments, two calls in one section, JSON flat
keys with a string id, unbalanced braces, nested keys with an integer id,
function-as-key layout, arguments before the name, a schema-declared string value,
whitespace-only reasoning, an argument name with a leading and with a trailing
space, a Python-style value that must normalize to JSON, and a tool name with a
trailing space.

## Approximations recorded (not hidden)

1. The port keeps one tool buffer where native keeps one string per field; it
   needs a high-water cursor and an anchored empty buffer, otherwise the first
   call's arguments bleed into the second.
2. JSON escaping does not validate UTF-8: native `ordered_json(...).dump()`
   replaces invalid sequences with U+FFFD, the port passes bytes >= 0x20 through.
3. `normalize_quotes_to_json` uses ASCII word characters; native `isalnum` is
   locale-dependent.
4. Buffer capacity is the caller's contract: the port truncates instead of
   growing a `std::vector`.
5. Branches the gate cannot reach: the final flush of a named pending call (the
   base mapper pushes at `tool-name`), and `tool-arg-string-value` from the
   model-specific parsers, which the fixture builds by hand.
6. `arguments` receives raw text and is never re-marshalled, so a native
   `{"city":Paris}` is preserved exactly; that is the original behaviour.

## Still open

gemma4/minimax mappers, partial streaming, the tool builders, the typed
direction, autoparser/grammars, model parsers, `use_jinja=false` and the chat
API's missing time injection.
