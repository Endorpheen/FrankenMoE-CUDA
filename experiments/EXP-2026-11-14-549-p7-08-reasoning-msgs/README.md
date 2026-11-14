# EXP549: reasoning_content in messages (P7-08 slice 5, part 39)

## Hypothesis

`reasoning_content` is a plain message field: `to_json_oaicompat` emits it after
`content` when non-empty, and templates that understand it print it. The ported
message normalizer dropped it, so any reasoning template saw nothing. Adding the
field to `chat_normalize` should close the gap, gated against
`common_chat_templates_apply` with the same reasoning-bearing message.

## What the investigation changed

The first attempt gated `Ministral-3`, `GLM-4.6` and `deepseek-r1`. The gate was
green, but the mutations were almost all invisible — which exposed that those
three templates do **not** consume `reasoning_content` in the way the test
assumed:

| template | finding |
|---|---|
| `mistralai-Ministral-3-14B-Reasoning-2512` | served by a dedicated C++ prompt builder (`params_init`), not the jinja path |
| `GLM-4.6` | on the jinja path but ignores `reasoning_content` |
| `llama-cpp-deepseek-r1` | on the jinja path but ignores `reasoning_content` |
| `zai-org/GLM-5.3` | on the jinja path and **uses** `reasoning_content` |

A probe that renders each template twice, with and without reasoning, showed
`USED` only for GLM-5.3. The reasoning gate therefore runs on the official
`GLM-5.3` template, stored as a test fixture (`tests/templates/GLM-5.3.jinja`,
downloaded from `huggingface.co/zai-org/GLM-5.3/raw/main/chat_template.jinja`).
The other two stay as negative controls: they must render identically with and
without reasoning, which the same helper checks.

## Rule added

`chat_normalize` now emits `"reasoning_content"` after `"content"`, only when the
reasoning text is non-empty, matching `to_json_oaicompat`.

## Gate

```
FG549_RUNTIME_REASONING cases=570 values=1002 failures=0 skipped=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=570 values=1002 failures=0 skipped=0
```

Cases added: two reasoning renderings on GLM-5.3 (with and without reasoning) and
one each on the two negative controls, plus two shape comparisons of the
normalized JSON against `common_chat_msgs_to_json_oaicompat` (with and without
reasoning). The shape comparison was added because output-level gating cannot see
an empty `reasoning_content` that templates skip anyway.

## Not ported

- `continue_final_message = reasoning_content` and reasoning deltas.
- `strftime_now`, the typed-content direction, grammars/autoparser, string
  marking/caps, model parsers and the legacy `use_jinja=false` path.
