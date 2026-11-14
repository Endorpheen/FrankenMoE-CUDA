# EXP536: reasoning caps flags (P7-08 slice 5, part 26)

## Hypothesis

caps.cpp checks two reasoning capabilities with different mechanisms:

- `supports_preserve_reasoning` renders a five-message history where a deep
  assistant message carries a placeholder reasoning, with `enable_thinking=true`
  and `preserve_thinking=true`, and asks whether the placeholder reaches the
  output (stats are explicitly not used, because a template may test reasoning
  without printing it);
- `supports_reasoning_effort` renders a one-message history with
  `enable_thinking=true` and `reasoning_effort="low"` and asks whether the
  `reasoning_effort` value was used.

Both are observable through our existing machinery: the output check is already
in the harness, and the effort check needs the usage mask of a context variable,
so `check_tpl` gained an `out_var` mask computed with `chat_probe_mask`.

That held, after two plumbing fixes found by the gate: the caps mode returned
before filling the rendered text, and the mask block did not run when only the
variable mask was requested.

## Rules captured

| flag | rule |
|---|---|
| `supports_preserve_reasoning` | the deep-history reasoning placeholder appears in the rendered output |
| `supports_reasoning_effort` | the `reasoning_effort` context value was used |

## Gate

```
FG536_RUNTIME_REASON cases=428 values=789 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=428 values=789 failures=0
```

Three cases: a template that prints reasoning content when present (preserve
only), `{{ reasoning_effort }}` (effort only), and a plain template (neither).

Mutations: rendered values losing the used bit, strings rendering as nothing,
identifiers resolving to undefined, the `if` condition inverted, object lookup
always missing, empty static member names, a one-item loop, the loop variable
bound to the whole array, the variable mask returning zero / shifted / masked
out, and rendering marking only the first value.

## Approximation

The native runtime binds `reasoning_effort` and `reasoning_strength` to the same
value, so using either counts. The fixture binds only `reasoning_effort`; a
template that reads only `reasoning_strength` would therefore be reported as not
supporting effort. This is recorded as a limitation, not silently ignored.

## Not ported

- Typed content normalization, tool schemas/grammars, the autoparser, string
  marking/caps, strftime_now and per-model parsers.
