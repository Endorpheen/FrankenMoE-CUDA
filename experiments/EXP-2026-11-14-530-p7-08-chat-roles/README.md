# EXP530: chat role workarounds in Goose (P7-08 slice 5, part 20)

## Hypothesis

The caps rule that decides the system-role workaround is dynamic: `caps.cpp`
renders a probe with `[system "System message", user "User message"]` and clears
`supports_system_role` when `messages[0].content` was not used. For a harness we
can approximate that decision with a differential probe: render the template with
two different system contents; if the outputs are equal, the content was not
used. With that flag supplied, `map_developer_role_to_system` and
`system_message_not_supported` are deterministic and portable on top of the
EXP529 message shape.

That held. The exported `chat_normalize` maps `developer` to `system`, then, when
the flag says the template does not support the system role, merges the first
system message into the next one with a newline (or drops it when it is the only
message), and serializes the resulting message list as JSON.

## Order matters

The mapping has to run before the workaround, and the gate proved it: with the
check applied to the raw first role, `developer`+`user` stayed two messages and
rendered `system;user;` while the native side renders `user;`. The shipped code
checks for `system` or `developer` as the first role.

The same pool rule as in EXP519 applies: the array slots are reserved before the
message objects are built, otherwise the second child lands outside the array's
contiguous range and the JSON contains a stray key.

## Harness

| piece | rule |
|---|---|
| native oracle | `common_chat_templates_apply(...).prompt.str()` |
| caps proxy | two probe renders with `System message` and `ZZZ`; equal output means unsupported |
| our side | `chat_normalize` builds the JSON, the fixture turns it into template values and renders with the Goose runtime |
| comparison | byte-for-byte on the final string |

Six cases: role/content loop with `system`+`user`, role-only with `system`+`user`
(merge), role-only with a lone `system` (drop), `developer`+`user` on both
templates (mapping), `system`+`user` with empty system content, and a template
that renders only non-system content so the merged text is visible.

## Gate

```
FG530_RUNTIME_ROLES cases=385 values=762 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=385 values=762 failures=0
```

Mutations: developer unmapped, merge joined by a space, merged content dropped,
lone system kept, merge flag not set, start not advanced, caps ignored, wrong
role in the developer check, wrong mapped role, effective count off by two, only
one array slot reserved, and swapped object keys.

## Not ported

- `jinja::caps::analyze` itself: the harness supplies the flag through a
  differential probe, which matches the native stats rule for these templates but
  can disagree for templates that read a value without affecting the output.
- Typed content normalization, tool calls, reasoning content, tools/grammar and
  the autoparser.
