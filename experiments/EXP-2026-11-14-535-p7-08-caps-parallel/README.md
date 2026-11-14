# EXP535: parallel-call and object-argument caps flags (P7-08 slice 5, part 25)

## Hypothesis

The two remaining tool probes are variations of EXP534's context:
`supports_object_arguments` reads
`messages[1].tool_calls[0].function.arguments.arg` in the object-arguments probe,
and `supports_parallel_tool_calls` reads
`messages[1].tool_calls[1].function` in a two-call probe (a failed render means
false). The `obj_get`/`arr_at` helpers and the usage mask should cover both, with
one missing piece: the native detector marks *intermediate* access results used,
so a member access has to mark its container.

That held. `chat_probe_tool_arg_mask` and `chat_probe_second_call_mask` walk the
two paths, and `eval_member` now marks the accessed container, which is what makes
the second call's `function` object count as used.

## Rules captured

| flag | rule |
|---|---|
| `supports_object_arguments` | the object arguments probe succeeded and `tool_calls[0].function.arguments.arg` was used |
| `supports_parallel_tool_calls` | the two-call probe succeeded and `tool_calls[1].function` was used |

## Gate

```
FG535_RUNTIME_PAR cases=419 values=786 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=419 values=786 failures=0
```

Three cases: a template reading the first call's object argument (object
arguments only), a template reading the second call's function name (parallel
only), and a plain template (neither).

Mutations: the tool argument reading the calls array, the message, the second
call, a misspelled `arg` key or `arguments` key, reporting unused; the second
call reading the first, reporting unused, a misspelled `function` or
`tool_calls` key; member access stopping the container mark or marking the
property instead.

One candidate mutation (dropping the "arguments must be an object" guard) was
removed: the probe always uses object arguments, so it was unobservable.

## Not ported

- `supports_preserve_reasoning` and `supports_reasoning_effort` probes.
- Typed content normalization, the autoparser, string marking/caps,
  strftime_now and model parsers.
