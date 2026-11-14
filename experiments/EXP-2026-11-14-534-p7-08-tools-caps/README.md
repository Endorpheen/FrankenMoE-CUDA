# EXP534: tools caps flags (P7-08 slice 5, part 24)

## Hypothesis

caps.cpp's tools probe renders a five-message conversation with a `tool_calls`
entry and a `tools` array, then reads two usage flags:
`tools[0].function.name` (supports_tools) and `messages[1].tool_calls`
(supports_tool_calls). With the usage flag from EXP531 and the operation mask
from EXP532, a path lookup over the same probe context should reproduce both
flags.

That held. The Goose module gained `obj_get`/`arr_at` helpers and two exports
that walk the exact paths caps.cpp inspects; the harness builds the probe context
as template values, renders in caps mode and compares the computed flags with
`common_chat_template::original_caps()`.

## Rules captured

| flag | rule |
|---|---|
| `supports_tools` | false when the render failed or `tools[0].function.name` was not used |
| `supports_tool_calls` | false when the render failed or `messages[1].tool_calls` was not used |

## Gate

```
FG534_RUNTIME_TOOLS cases=410 values=783 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=410 values=783 failures=0
```

Three cases: a template that prints the tool names (tools only), a template that
indexes `tool_calls` (both flags), and a plain template (neither).

Mutations: the object walk matching any key or returning the key id, reading the
tool name from `description`, the key length read as an offset, the tool-calls
key misspelled, array indexing returning the first child or rejecting
containers, the tool name read from the second tool or reported unused, the tool
calls read from the first message or reporting the message mask, and the
function key misspelled.

Two candidate mutations (dropping the object-kind guard and the array bounds
check) were removed because the probe context never exercises them; keeping them
would have been an unobservable claim.

## Not ported

- `supports_parallel_tool_calls`, `supports_object_arguments`,
  `supports_preserve_reasoning`, `supports_reasoning_effort`: each has its own
  probe with different contexts and argument shapes.
- Typed content normalization, the autoparser, string marking/caps,
  strftime_now and model parsers.
