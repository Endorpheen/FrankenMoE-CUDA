# P7-08 output mapper notes

Working notes for the next slice (base AST mapper). Every statement below was
read in the native tree and spot-checked against the code; the line numbers refer
to `llama.cpp-flint-cuda` at the time of writing. Nothing here is ported yet.

## Entry point and lifecycle

- `common/chat.cpp:1672-1683` builds `common_chat_msg`, sets `msg.role =
  "assistant"` and selects the mapper by format: `PEG_GEMMA4`,
  `PEG_MINIMAX_M3`, otherwise the base `common_chat_peg_mapper`. The mapper never
  touches the role.
- Streaming does not carry mapper state: `feed()` appends the chunk to the
  accumulated input and re-parses the whole text with a fresh mapper
  (`common/chat.cpp:1753-1761`). A mapper is therefore a pure function
  `AST -> common_chat_msg`, but intermediate results must stay monotonic.
- On a partial parse failure with `result.end > 0` the mapper still runs on the
  partial AST (`common/chat.cpp:1644-1666`).

## Text accumulation

- `content` and `reasoning` append `sanitized_text()` with no separator and no
  trim (`common/chat-peg-parser.cpp:393`), so two adjacent nodes concatenate
  directly.
- Helper `trim()` trims **one** leading space (`trim_leading_space(sv, 1)`) plus
  trailing space (`common/chat-peg-parser.cpp:36-38`). That asymmetry is
  intentional in the current code and must be copied, not "fixed".
- `normalize_container_value` is declared but unused by the base mapper
  (`common/chat-peg-parser.h:21`).

## Tool calls

- `tool-open` resets `pending_tool_call`, points `current_tool` at it, clears
  `arg_count`, `args_buffer` and `closing_quote_pending`
  (`common/chat-peg-parser.cpp:342-349`).
- `tool-id` trims trailing space and strips one pair of surrounding double
  quotes (`:351-357`).
- `tool-name` sets the name from trailing-space-trimmed text, then:
  arguments take `args_buffer` if it is non-empty, otherwise `"{"` when empty;
  the pending call is pushed into `result.tool_calls` immediately (so streaming
  sees it) and `current_tool` points at that pushed entry (`:359-372`).
- `tool-args` takes a complete JSON object only when the text starts with `{`
  (`:374-381`).
- `arg-name` writes `,` when `arg_count > 0`, then `ordered_json(trim(text)).dump()
  + ":"`; the target starts with `{` when empty (`:388-400`).
- `arg-value` goes through `normalize_quotes_to_json` (Python quotes,
  `True`/`False`/`None`), and schema-declared string values use
  `escape_json_string_inner` instead of the JSON marshaler.
- `tool-close` closes the pending quote and the `{` by `json_brace_depth`.
- `reasoning-block` is a dead tag in the base mapper (`common/chat-peg-parser.h:63`).

## Subclasses

- `gemma4` builds arguments from AST **rule names** (`gemma4-*`) rather than tags
  and honours `is_partial`; it does not support tool ids.
- `minimax_m3` builds arguments from schema containers
  (`tool-arg-object`, `tool-arg-array`, `tool-arg-item`) via
  `minimax_m3_container_to_json` and also honours `is_partial`.

## Differential inputs without weights

The oracle is `common_chat_session(tmpls, nullptr, inputs).finish(text)`, as
established in EXP555. For the base mapper the useful shapes are: XML-style tool
tags, JSON with nested keys and an `id`, flat keys with a numeric id, arguments
that arrive before the name, Python-style calls, two tool calls in one message,
a message with reasoning before content, and an empty arguments object.

## Porting risks

1. Keep the traversal order: `args_buffer` is only flushed at `tool-name`.
2. Do not rebuild `arguments` through a JSON marshaller; the native code appends
   strings, and escaping differs per branch.
3. Copy `trim()` semantics exactly (one leading space).
4. `is_partial` changes output for gemma4/minimax; the base mapper ignores it.
5. The tool call is pushed at `tool-name`, so a later `tool-open` starts a new
   entry rather than extending the previous one.
6. `escape_json_string_inner` is used for schema-declared string values only.
