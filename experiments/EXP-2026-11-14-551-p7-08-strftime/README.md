# EXP551: strftime_now with an injectable clock (P7-08 slice 5, part 41)

## What the code says

`strftime_now` formats `ctx.current_time` with `std::localtime` + `std::strftime`
(`common/jinja/value.cpp`). `context.current_time` is `std::time(nullptr)` in the
constructor and copied from the parent scope (`common/jinja/runtime.h`). The chat
layer passes `inputs.now` only to the autoparser `generation_params`
(`common/chat.cpp`, `params.now`); the jinja context never receives it, and
`extra_context` becomes template variables, not the clock.

So there is no way to give the native *chat* path a fixed time. But the function
itself is portable if the clock is a parameter, and the fixture can set
`ctx.current_time` directly for the native jinja oracle.

## Port

`strftime_now` is a builtin value in the Goose runtime. It formats through a
small extern shim to libc (`localtime` + `strftime`), so the formatting rules,
the locale and the time zone are exactly the native ones — no reimplementation of
strftime, no time zone tables.

The clock comes from the `now_epoch` context value when present, and from the
process clock otherwise. Production behaviour therefore matches the native
runtime; the value exists so a test can pin the clock.

## Gate

The native oracle is a `jinja::context` with `current_time` set to the same
epoch, executing the same `{{ strftime_now("...") }}` template. Eight formats:
`%Y-%m-%d`, `%d %b %Y`, `%H:%M:%S`, plain text, `x%Yy%m`, `%s`, `%j %U %w`,
`%Y|%m`, plus epoch 0.

```
FG551_RUNTIME_TIME cases=587 values=1036 failures=0 skipped=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=587 values=1036 failures=0 skipped=0
```

## Remaining limitation (recorded, not hidden)

`Mistral-Small-3.2` and `openai/gpt-oss-120b` stay out of the chat-level example
gate. Their templates call `strftime_now` at render time, and
`common_chat_format_example` offers no way to set the clock, so a comparison would
depend on the wall clock and could disagree at a day boundary. The function is
ported and gated deterministically; what remains unavailable is the *chat API's*
time injection, which is a property of the native layer, not of the port.

## Not ported

- The typed-content direction, grammars/autoparser, string marking/caps, model
  parsers and `use_jinja=false`.
