# EXP548: modern model templates (P7-08 slice 5, part 38)

## Hypothesis

The example gate covered five templates. Adding current official templates from
`models/templates/` should either confirm compatibility or name the missing
constructs. A second dimension — tools and the thinking switch — is already
ported (messages, roles, caps, tool schemas) and can be gated by calling
`common_chat_templates_apply` with a tool list and `enable_thinking` values.

## Templates added to the example gate

| template | outcome |
|---|---|
| `Qwen-Qwen3-0.6B.jinja` | passes |
| `Qwen3.5-4B.jinja` | passes |
| `Qwen3-Coder.jinja` | passes |
| `GLM-4.6.jinja` | passes |
| `GLM-4.7-Flash.jinja` | passes |
| `LFM2.5-Instruct.jinja` | passes |
| `mistralai-Mistral-Nemo-Instruct-2407.jinja` | passes after `selectattr` was added |
| `mistralai-Ministral-3-14B-Reasoning-2512.jinja` | passes |
| `unsloth-mistral-Devstral-Small-2507.jinja` | passes |
| `openai-gpt-oss-120b.jinja` | excluded: needs `strftime_now` |

Apex and LFM3 have no official template in the tree (only LFM2.5), so they cannot
be gated without inventing one, which the task forbids.

## Gaps fixed

1. `selectattr` / `rejectattr` were unknown filter names; the runtime only had
   `select` / `reject`. They are the same handler in the native tables, so the
   names were added to the dispatch and to the array method table.
2. `tojson(indent=2)` was ignored, so a tool list rendered compact while the
   native side pretty-printed it with two spaces. `tojson` now reads the
   `indent` keyword or the second positional argument and switches to the
   `","` / `": "` separators, as the native default does for `indent >= 0`.

## Tools and reasoning dimension

Five templates are rendered through `common_chat_templates_apply` three ways each:
no tools with thinking on, one tool with thinking on, and no tools with thinking
off. Our side builds the context with the ported message normalizer, the ported
tool serializer and the `enable_thinking` value, so the comparison covers the
whole chain rather than the template alone.

## strftime_now finding

The earlier deferral is confirmed with evidence: `inputs.now` reaches only the
autoparser `generation_params`, while `jinja::context.current_time` is set from
`std::time(nullptr)`. There is therefore no way to give the native side a fixed
time, so `strftime_now` cannot be gated deterministically. It stays unported and
both templates that call it are excluded by name.

## Gate

```
FG548_RUNTIME_MODERN cases=561 values=986 failures=0 skipped=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=561 values=986 failures=0 skipped=0
```

Fourteen example templates and fifteen apply cases (five templates times
{tools off, thinking on} / {one tool, thinking on} / {tools off, thinking off}).

Mutations: `selectattr` not accepted, `rejectattr` behaving like `selectattr`,
the array method table losing `selectattr`, `tojson` ignoring its indent (x2),
keeping the compact separators, dropping the indent value, a deeper indent, a
lost key-separator space, ignoring the keyword, ignoring the positional argument
and reading the first positional as the indent.

Two corpus gaps were found by the mutation round: no template used
`rejectattr`, and nothing passed `indent` positionally. Direct cases were added
for `[o]|selectattr('k')`, `[o]|rejectattr('k')`, `([o, v]).selectattr('k')`,
`v|tojson(none, 2)` and `v|tojson(true, 1)`.

## Kept

All earlier cases are unchanged; this slice only adds templates and cases.
