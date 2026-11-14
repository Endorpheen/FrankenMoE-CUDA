# EXP528: chat-layer harness (P7-08 slice 5, part 18)

## Hypothesis

The chat layer cannot be gated by the template fixture because the fixture renders
a template with a context it builds itself, while the chat layer builds the
context (`messages`, `add_generation_prompt`, caps-driven workarounds) and renders
internally. A separate harness is therefore needed: call
`common_chat_templates_apply` with fixed inputs for the native expectation, and
compare it with our runtime rendering the same template from an equivalent
context.

That works. `llama_chat_apply_template` is not the right entry point: it only
detects built-in template strings and returns -1 for a custom one. The jinja path
is `common_chat_templates_init(nullptr, tmpl)` plus `common_chat_templates_apply`,
which lives in `libllama-common.so`; linking that library into the fixture makes
the oracle available without a model.

## Harness

| piece | rule |
|---|---|
| native oracle | `common_chat_templates_init(nullptr, tmpl)` then `common_chat_templates_apply(inputs).prompt.str()` with `common_chat_msg` entries |
| our side | the same template rendered by the Goose runtime with `messages` (array of objects with `role`/`content`) and `add_generation_prompt` in the context |
| comparison | byte-for-byte, same failure reporting as the template gate |

Five cases: one user message with and without the generation prompt, two messages,
`loop.index` over the messages, and `messages|length` with a `default` on an empty
content.

## Finding that shapes the next slice

Passing `system` + `user` produced `1:user;` from the native side: the chat layer
mutates the message list before rendering (under the default capabilities the
system role is not supported, so `workaround::system_message_not_supported`
merges or drops it, and `developer` is mapped to `system`). The fixture, which
builds `messages` directly, cannot reproduce that. The context-construction port
(message structs, caps workarounds, role mapping) is therefore the next slice;
this one establishes the oracle and proves the template side matches the chat
output byte-for-byte for message lists the workarounds do not rewrite.

## Gate

```
FG528_RUNTIME_CHAT cases=370 values=740 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=370 values=740 failures=0
```

Mutations target the runtime paths the chat cases exercise: `loop.index`,
`loop.length`, object key matching, the `default` boolean check, the `if` body,
array length, computed members, object member lookup, statement order, the last
array index, static member names and the empty-string default rule.

## Not ported

- The message-to-context construction (caps workarounds, role mapping, JSON
  message rendering) is still the chat layer's; it is the next slice and needs
  this harness for the native expectation.
- Tool schemas, grammars, `strftime_now` and thinking/reasoning fields.
