# EXP529: chat message shape in Goose (P7-08 slice 5, part 19)

## Hypothesis

`render_message_to_json` starts with `to_json_oaicompat` per message, i.e. the
`{"role": ..., "content": ...}` object that the chat layer hands to the template.
That shape is deterministic and can be ported and gated against the native test
helper `common_chat_msgs_to_json_oaicompat`, which builds the same JSON without a
model. The caps-driven workarounds that run later need the caps analyzer and are
a separate slice.

That held for the shape. The export `chat_message_json` builds the object with
the module's own value model and serializes it through `json_into`, so the port
shares the existing JSON writer instead of a second escaper.

## Harness

| piece | rule |
|---|---|
| native oracle | `common_chat_msgs_to_json_oaicompat({msg}, false).dump()` from `libllama-common` |
| our side | `chat_message_json(A, F, B, E, st, role, content, out)` on a fresh arena |
| comparison | both strings parsed with `common_json` and compared structurally, so `", "` versus `","` formatting does not matter |

Eight cases: ordinary, empty content, a quote, a newline, a tab, a backslash, a
tool role with `{}` content, and a byte-exact UTF-8 payload.

## Why the workarounds are not in this slice

The native order is: build the JSON shape, map `developer` to `system` (except
for gpt-oss templates), then, when the template caps do not support the system
role, merge the first system message into the next one or drop it. The caps come
from `jinja::caps::analyze`, which runs probe templates through the runtime and
reads value usage statistics (592 lines), and the chat layer additionally
generates a PEG parser for every template:

- probing `{{ messages|tojson }}` through `common_chat_templates_apply` throws
  `Unable to generate parser for this template`;
- a `system`+`user` list renders as a single `user` message.

Both are recorded as the next slice, because without caps detection the merge
rule cannot be decided.

## Gate

```
FG529_RUNTIME_MSGJSON cases=378 values=748 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=378 values=748 failures=0
```

Mutations: role/content key names, key order, role/content value swap, the bare
object instead of the array, a duplicated message, a literal content, an empty
role, a truncated content, an empty object in the array and a duplicate key.

## Not ported

- Caps analysis and the caps-driven workarounds (`developer` mapping is trivial
  once caps is known; the system merge/drop needs `supports_system_role`).
- Typed content normalization, tool calls, reasoning content, tools/grammar, and
  the autoparser.
