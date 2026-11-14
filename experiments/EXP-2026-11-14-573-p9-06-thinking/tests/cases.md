# P9-06 S3 cases: thinking control

Oracle: native `bmoe::detail::probe_think_control` and
`bmoe::detail::add_no_think_prefill` from
`runtime/core/src/engine/thinking_control.cpp`, compiled into the fixture. The
port is `think.goose`; the gate is `tests/fixture.cpp` + `tests/build.sh`.

The native probe needs `common_chat_templates_apply` and
`common_chat_templates_source`; those are **not** ported. The fixture uses the
native renderer to produce the three prompts (off, on, prefilled) and feeds
them, the template source and its tags, to the pure Goose `tc_probe`. This is
the recorded dependency, not a hidden one.

## Cases (12)

| # | case | template | expected | source of the expectation |
|---|---|---|---|---|
| 1 | qwen3 | Qwen3.5-4B.jinja | Template | test `qwen3: template honours enable_thinking`; `on != off` |
| 2 | lfm2.5 | LFM2.5-8B-A1B.jinja | None | test `lfm2.5: probed as none`; declares `<think>`, prefill lands but does not move past the close |
| 3 | gpt-oss | openai-gpt-oss-120b.jinja | Prefill | test `gpt-oss: probed as prefill`; no declared start tag, prefill moves the render |
| 4 | lfm2 | LFM2-8B-A1B.jinja | Template | test `non-reasoning lfm variant ...`; declared tag is never used by the template |
| 5 | lfm2.5-instruct | LFM2.5-Instruct.jinja | Template | same test; family declares the tag, template does not use it |
| 6 | gemma4 | google-gemma-4-31B-it.jinja | Template | test `gemma4: template honours enable_thinking` |
| 7 | plain | inline one-line template | Template | test `plain template: nothing to suppress`; no span, flag inert, prefill absent |
| 8 | null | no template | Template | test `null templates: fails open to template` |
| 9 | prefill construction | none | assistant, content `""`, reasoning `"\n\n"`, continuation `CONTENT` | `add_no_think_prefill` (`thinking_control.cpp:52-60`) |
| 10 | prefill bytes | none | length 2, bytes `\n\n` | `kEmptyReasoning` (`:27-29`) |
| 11 | lfm2.5 span closed | LFM2.5-8B-A1B.jinja | prefilled contains `</think>`, no fresh `<think>` after it | test `expect_span_closed` (`think_control_test.cpp:96-107`) |
| 12 | names | none | `template`, `prefill`, `none` | `think_control_name` (`thinking_control.cpp:9-19`) |

Cases 1-8 compare the native probe result and the port result directly. Cases
9-12 check the prefill construction, the constant bytes, the span property and
the name table.

## Mechanism, as ported

1. **Template.** If `on != off` the template reads `enable_thinking`; nothing
   is added. Also returned for a null template, a thrown probe, a template with
   no reasoning span and no structural reasoning, and a non-reasoning member of
   a family that declares a tag but never emits it.
2. **Prefill.** The template ignores the flag, has no declared start tag, and
   the prefilled render differs: reasoning is structural (harmony), so starting
   the turn past that section cannot be declined.
3. **None.** The template ignores the flag and declares a start tag that its
   source actually contains. If the prefilled render moves past the last
   closing tag into non-whitespace (another channel/header), the model is
   placed in its answer section and the result is **Prefill**; otherwise the
   closed empty span is only a suggestion and the result is **None** (LFM2.5,
   issue #82).

The prefill is the exact native shape: append an assistant message with empty
content and reasoning `"\n\n"` (whitespace, so the engine contributes no
words), then `continue_final_message = CONTENT` (3). AUTO is wrong here: it
resolves a reasoning-only message to a REASONING continuation, which opens the
span instead of closing it.

## Expected fixture line

```
FG577_THINK cases=12 values=26 failures=0
```

`26 = 7 probe cases x 2 (control + name) + null (1) + prefill construction (5)
+ prefill bytes (2) + span (1) + names (3)`.
