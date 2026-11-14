# EXP532: content caps flags (P7-08 slice 5, part 22)

## Hypothesis

`caps.cpp` derives `supports_string_content` and `supports_typed_content` from
one probe: it renders `[user with content "STRING_MARKER"]` and inspects the
value's operation history. Typed content is supported when the content was used
through `selectattr` or an array access; string content is unsupported when the
content was used as an array and the marker is missing from the output. With the
usage flag from EXP531 turned into a small operation mask, the same decision can
be computed from the Goose runtime's observations.

That held. The mask bits are: 1 = rendered, 2 = indexed, 4 = select/reject
input, 8 = `is string` operand. The harness reproduces the caps decision from
the mask plus the rendered text and compares it with
`common_chat_template::original_caps()`.

## What changed

| change | rule |
|---|---|
| mask | `st[27]` is now a bit set; rendering ORs bit 1 instead of assigning |
| indexing | array/tuple and string indexing OR bit 2 on the indexed value |
| select/reject | the filter input gets bit 4 |
| `is string` | the operand gets bit 8 |
| exports | `chat_probe_mask` and `chat_probe_content_mask` (the content walk of EXP531, returning the mask) |

## Gate

```
FG532_RUNTIME_CAPS2 cases=400 values=784 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=400 values=784 failures=0
```

Three content cases match the native flags: plain `{{ messages[0]['content'] }}`,
`{{ messages[0]['content'][0] }}` (array access, marker missing), and
`{{ messages[0]['content']|string }}`.

Mutations: lose the used bit, lose the indexing bit, make rendering set the
array bit, content mask returning zero / adding the array bit / adding the
select bit / dropping the array bit / shifting the bits, the key walk matching
any key or any length, and the walk returning the key mask or the object mask.

## Gaps found while probing

Two caps probe templates cannot be gated yet because the Goose runtime rejects
what native accepts:

- `{% for c in messages[0]['content'] %}`: our `for` raises "Expected iterable or
  object type ... got String";
- `{{ ...|select('gt', 0) }}` on a string: our `select` requires an array.

Both are recorded as the next runtime slices; they matter for real chat
templates too.

## Not ported

- `supports_tools` and `supports_tool_calls`: their probe needs a tools context
  and nested usage checks (`tools[0].function.name`, `messages[1].tool_calls`,
  the call arguments).
- The `is string` bit is marked but no flag uses it yet (caps uses it only to
  choose a second probe).
