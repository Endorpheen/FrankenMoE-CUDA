# EXP563: legacy chat path (`use_jinja=false`) (P7-08 slice 8)

## Hypothesis

The legacy path (`common_chat_templates_apply_legacy`, `llama_chat_apply_template`)
is a set of hand-written builders selected by a substring detector. It can be
gated without weights: `llm_chat_apply_template` is a pure function of the
template source and the message list.

## Oracle

Two independent probes with real output, no model and no weights:

- `probe` links the real `src/llama-chat.cpp` and reproduces the wrapper from
  `src/llama.cpp:508-529` verbatim, so detection and prompt building run exactly
  as in the server;
- `probe2` links `libllama-common` and calls `common_chat_templates_apply` with
  `use_jinja=false` and `true`, printing every `common_chat_params` field.

Detection: 55/55 names. Prompts: 34 template cases + 39 targeted cases + 2
content-part cases, all byte-identical.

## Port

`legacy_detect` and `legacy_apply` plus the builders for all 55 formats, in the
canonical runtime. The draft came from a helper agent and was rebuilt here before
integration; the older 36-format draft was rejected in favour of the complete one.

## Gate

```
detection: 55/55 identical
templates: ok=34 gaps=0 bad=0
cases: ok=39 gaps=0 bad=0
parts: ok=2 gaps=0 bad=0
mutations 9/9 caught (half 1)
mutations 9/9 caught (half 2)
```

## What the legacy path does differently

- It ignores `tools`, `tool_calls`, `tool_name`, `tool_call_id` and
  `reasoning_content` entirely (`common/chat.cpp:3751-3782`), and the format
  stays `CONTENT_ONLY`.
- It emits no `generation_prompt`, no parser arena and no message delimiters;
  the jinja path does (`genp_len=22`, `parser_len=10`, `delims=2` in the probe).
- It ignores `add_bos`/`add_eos`, while jinja strips BOS/EOS.
- Its `trim` is C `isspace` including `\v` and `\f`; Go's `std.trim` does not
  remove those.
- Content parts are merged with a newline before each part, including media —
  the known upstream duplication quirk is copied, not fixed.

## Risks recorded

1. Detection is a substring heuristic with a hard order; real templates already
   land in neighbouring families (pre-v1 Mistral → LLAMA_2, OpenHermes → CHATML).
2. The buffer contract is not reproduced: native sizes `sum*1.25`, copies with
   `strncpy` and retries; the port returns the full length in one call.
3. The legacy path loses tool support by design; the gate cannot cover what the
   native path does not build.
4. The `json_schema` grammar differs between paths (211 vs 977 bytes); only the
   legacy side is gated here.
5. The oracle is linked from the flint tree; if that tree moves, the goldens must
   be regenerated.
