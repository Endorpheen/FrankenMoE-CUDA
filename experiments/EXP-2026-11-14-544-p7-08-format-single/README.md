# EXP544: common_chat_format_single (P7-08 slice 5, part 34)

## Hypothesis

`common_chat_format_single` renders the history without the new message, renders
the history with it, and returns the tail of the second render beyond the length
of the first. Two details matter: when a generation prompt is requested and the
history render ends with a newline, that newline is preserved ahead of the tail;
and an empty history skips the first render entirely, so the tail is the whole
render.

That held. `chat_format_single(past, full, add_ass, out)` implements exactly that
composition.

## Boundary of this slice

The rendering itself is not reimplemented here. Both the native function and the
gate obtain `past` and `full` from `common_chat_templates_apply`, so the gated
piece is the composition rule (prefix subtraction, newline preservation,
empty-history case), which is the whole body of the function. The rendering path
that feeds it is already gated by EXP528-543.

The `use_jinja=false` branch is not covered: it routes to the legacy C++
template implementation, which is out of scope for this port. All cases call the
native function with `use_jinja=true`.

## Gate

```
FG544_RUNTIME_SINGLE cases=502 values=868 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=502 values=868 failures=0
```

Eight cases over two templates (one whose history render ends with a newline, one
without): empty history with and without the generation prompt, one past message
with and without it, two past messages, and a newline template with empty
history.

Mutations: newline never preserved, always emitted, ignoring `add_ass`, written
twice, turned into a space; the tail starting early, late, at zero or empty;
skipping every other byte; the cursor not advanced; and the loop bound off by one.

Two candidate mutations were dropped as unobservable by construction: reading the
last byte from `full` instead of `past` (identical, because `full` starts with
`past`), and the `past.len > full.len` shortcut (unreachable when `full` contains
`past` as a prefix).

## Not ported

- `use_jinja=false` (legacy template path).
- The rendering and context construction that produce the two strings; covered by
  earlier slices.
- The typed-content direction, grammars/autoparser, string marking/caps,
  `strftime_now` and model parsers remain native.
