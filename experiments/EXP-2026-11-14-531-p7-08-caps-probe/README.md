# EXP531: value usage counter foundation (P7-08 slice 5, part 21)

## Status: partial by design, blocker recorded

This slice was meant to port `jinja::caps::analyze`. The analysis is dynamic: it
renders probe templates and inspects `value_t::stats.used` on
`messages[0].content` (caps.cpp around the "system prompt" check). A Goose port
therefore needs a usage flag in the value model, and the flag has to be readable
after a render.

The foundation is done and gated; the detector itself is **not** verified and is
not committed as working. The blocker is recorded below.

## What is in this slice

| change | rule |
|---|---|
| arena | the value record gained an eleventh field, `st[27]`, holding the usage flag; `bases()` and `ast_bases()` were widened, and the fixture writes the pool at `11 * valcap` |
| marking | `append_rendered` sets the flag of every value it renders, approximating the native `stats.used` |
| probe exports | `chat_probe_used(A, st, id)` reads a flag, `chat_probe_content_used(A, B, st, msgs)` looks up `messages[0]["content"]` and returns its flag |

The existing 385-case gate stays green, so the arena change did not regress any
template behaviour:

```
FG531_RUNTIME_CAPS cases=385 values=762 failures=0
```

## Blocker

`chat_probe_content_used` returns 0 for every template, including
`{{ messages[0]['content'] }}` where the flag must be 1. The debug run showed
`messages` kind 4 (array), its first child kind 6 (object), so the shape is
right; the key walk inside the export does not find `content`. Three attempts
(key bytes read through `st[18]/st[19]`, a byte-compare helper, an id-based
debug export) did not isolate whether the key offsets or the value-id lookup is
wrong, and the caps comparison case was removed again to keep the tree green.

Per the repository rule the trial-and-error loop stops here. The next step needs
a decision from Igor: either consult the senior helper in Codex with this
report, or accept a different design for the flag (for example a host-side set
of used ids instead of an arena field).

## Files worth looking at

- `source/runtime.goose`: `bases()`/`ast_bases()` (11 fields),
  `append_rendered` marking, `chat_probe_used`, `chat_probe_content_used`.
- `tests/stmt_fixture.cpp`: arena sizing in `check_tpl`, `check_chat_shape`,
  `check_chat_roles`.
