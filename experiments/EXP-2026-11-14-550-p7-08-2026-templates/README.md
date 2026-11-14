# EXP550: 2026 model templates (P7-08 slice 5, part 40)

## Hypothesis

EXP548 gated templates that ship with this llama.cpp tree, some of them older
generations. The current-generation templates (November 2026) should either pass
on the ported engine or name what is missing. They are not all in the local
folder, so they were fetched from the model repositories.

## Sources

Fetched on 2026-11-14 from `huggingface.co/<repo>/raw/main/chat_template.jinja`
and stored under `tests/templates/`; only the template text is stored, no weights.

| fixture | repository |
|---|---|
| `GLM-5.3.jinja` | `zai-org/GLM-5.3` |
| `LFM3-1.7B.jinja` | `LiquidAI/LFM3-1.7B` |
| `Qwen3.8-Flash-Next.jinja` | `Qwen/Qwen3.8-Flash-Next` |
| `Mistral-Large-4-A52B.jinja` | `mistralai/Mistral-Large-4-1T-A52B` |

`openai/gpt-oss-120b` is still the newest gpt-oss and is unchanged from EXP548: it
calls `strftime_now`, which has no deterministic oracle, so it stays excluded.

## Which path each template takes

The chat layer serves some templates with a dedicated C++ prompt builder that
bypasses jinja entirely. The fixture now prints the path for every template, so a
green line does not hide which side was compared.

| template | path |
|---|---|
| `GLM-5.3` | **jinja** |
| `LFM3-1.7B` | dedicated builder |
| `Qwen3.8-Flash-Next` | dedicated builder |
| `Mistral-Large-4-A52B` | dedicated builder |
| earlier: gemma-2, deepseek-r1, SmolLM3, Qwen3-0.6B, GLM-4.6/4.7, Mistral-Nemo | jinja |
| earlier: gemma-4, LFM2, Qwen3.5, Qwen3-Coder, LFM2.5, Ministral-3, Devstral | dedicated builder |

For the dedicated ones the gate proves that our jinja render coincides with the
hand-written builder, which is the end-to-end chat behaviour, but it does not
prove the template alone — that distinction is stated rather than implied.

## Gap fixed

`noop_statement` had no encoder branch, so `LFM3` and `Qwen3.8` were skipped with
"unsupported AST node: Noop". It is a no-op like a comment and now maps to the
same node kind.

## Gate

```
FG550_RUNTIME_2026 cases=578 values=1018 failures=0 skipped=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=578 values=1018 failures=0 skipped=0
```

Added: four example renderings (one per fixture), four `common_chat_templates_apply`
runs on GLM-5.3 (tools on/off, thinking on/off) and one reasoning rendering.

Mutations target the paths the new templates exercise: developer mapping, reverse
slice default, slice walk bound, `loop.index`, loop count, the `if` body, object
member lookup, ternary condition, comment/noop handling, string slice bytes,
member assignment and the reasoning field.

## Not ported

- `strftime_now` (wall clock), the typed-content direction, grammars/autoparser,
  string marking/caps, model parsers, `use_jinja=false`.
- Multimodal message construction beyond the media-marker content shape from
  EXP538; the current templates do not exercise image parts in the example
  conversation.
