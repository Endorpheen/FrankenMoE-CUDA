# EXP-2026-11-14-573 — thinking control (P9-06 S3)

Native counterpart: `runtime/core/src/engine/thinking_control.cpp`. Base commit:
`378bf9a7`.

## What this slice covers

`source/think.goose` ports the pure decision layer of thinking control:

- `think_control_name` — `template`, `prefill`, `none`; an unknown code falls
  back to `template`, exactly like the native switch;
- `add_no_think_prefill` — append one assistant message with empty content and
  reasoning `"\n\n"`, then set `continue_final_message = CONTENT` (3);
- `probe_think_control` — the full decision tree: a null or failed probe gives
  Template; a differing flag gives Template; an owned span needs a non-empty
  start tag that the template source actually contains; an owned span with the
  prefill moved past the last end tag into non-whitespace gives Prefill,
  otherwise None; no span with a moved prefill and an empty start tag gives
  Prefill; everything else is Template;
- `tc_span_closed` — the test property: the prefilled prompt contains the close
  marker and no fresh start tag after it.

Two non-obvious native behaviours are reproduced verbatim: None requires the
template source to contain its declared start tag (a family tag alone is not
enough), and the prefill-lands path for an owned span returns Prefill only when
the render moves past the close marker into further structure.

## Gate

`tests/build.sh` builds the port at `-O1`, compiles the native
`thinking_control.cpp` and the fixture, and links `libllama-common` for the
renderer (`common_chat_templates_apply`). The fixture uses the native renderer
for the three prompts and compares the pure decision with the native probe over
six vendored templates, a plain template, null and four mechanism checks.

```
FG577_THINK cases=12 values=26 failures=0
```

The renderer itself is not ported; `tc_probe` takes the rendered prompts, the
template source, the start tag, the end tags and the null/failed flags as
inputs, so the differential covers the decision logic precisely.

`tests/native_thinking_control.cpp` is a copy of the runtime source with one
API adaptation only: `common_chat_params::prompt` is a `common_chat_prompt`
structure in the current llama.cpp base, so the three reads become
`.prompt.str()`. No logic was changed.

## Mutations

`tests/mutate.py --half 1|2` holds 6 + 6 textual mutations; each must pass
`goose check` and then fail the fixture.

```
python3 tests/mutate.py --half 1   # 6/6 caught
python3 tests/mutate.py --half 2   # 6/6 caught
```

## Recorded limits

- The session wiring (`session.cpp` probe/prefill) and the CLI surface stay
  native.
- The renderer (`common_chat_templates_apply`) is a native dependency; when the
  P7-08 chat-apply entry point lands, the fixture can drop the native render
  side.
- The gate runs at `-O1`.
