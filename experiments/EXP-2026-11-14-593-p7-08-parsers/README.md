# EXP-2026-11-14-593 — nine model-output parsers (P7-08 slice 15)

Native counterparts: `common/parsers/{translate-gemma,functionary-v3-2,gigachat-v3,muse-glimmer,muse-spark,minicpm5,k2-horizon,harmony-xml,llm-jp-harmony}.cpp`.
Base commit: `740bd6c3`; the block was rebased onto the current canon and then
fixed.

## What this slice covers

The port adds nine model-output parser families to the canonical runtime, each
taking the model continuation and returning one `common_chat_msg` as compact
JSON in the native field order:

- TranslateGemma, Functionary v3.2, GigaChat V3, Muse Glimmer, MiniCPM5,
  Harmony XML, Muse Spark, K2 Horizon, LLM-jp Harmony.

Five families were already in the draft; the rebase brought them onto the
current 17575-line canon (19025 lines after the block). Four parser defects were
found and fixed:

1. Muse: `<|eom|>` and `<|eot|>` are seven bytes, but the port advanced the
   remainder by six, leaving a `>` that broke every exact prefix check. This
   caused the empty content in muse-glimmer-reasoning, the dropped `Done!` tail
   in muse-spark-commentary and the missing calls in muse-spark-tool. All five
   sites (analysis, second `to=self`, commentary, user, ATEM tail) now consume
   the full token.
2. K2 Horizon XML: the call branch did not write the opening `{` and sliced the
   arguments from the first parameter. The native code wraps them with
   `p.tool_args`; the port now emits `call_start` plus `d_params_begin` and
   slices from the call start, like Muse and MiniCPM.

## Gate

`tests/build.sh` builds the runtime with `--standalone` at `-O0` and links
`tests/fixture.cpp` against the native chat session (`common_chat_templates_init`
+ `common_chat_session(..., nullptr, ...)`); no weights and no model.

```
FG572_PARSERS cases=18 values=99 failures=0
```

18 cases over the nine families: content, reasoning, one and two tool calls,
argument typing, partial input, trailing junk and a malformed exponent.

## Mutations

`tests/mutate.py` holds 12 mutations in two halves; each runs in its own
directory with the same `source/` + `tests/` layout at `-O0`.

```
muse/ATEM half:  6/6 caught
k2/shared half:  6/6 caught
```

## Recorded limits

- Only the 18 gated paths are proven; the second `to=self` block is still not
  concatenated because no case covers it.
- One ATEM invoke per message; argument buffers are fixed at 16 KiB.
- The "arguments without escaping" mutation is equivalent for this fixture and
  is not counted; a parameter-separator check replaced it.
