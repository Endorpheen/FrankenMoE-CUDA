# EXP279: chat-endpoint and usage checks for the P8-04 compatibility suite

Extends EXP278 with the OpenAI-compatible chat surface, run against the accepted staging runtime.

## Checks and results

| check | expectation | result |
|---|---|---|
| chat non-stream | `finish_reason` `length` at `max_tokens` | pass |
| chat non-stream body | non-empty answer (content **or** reasoning) | pass |
| chat stream | concatenated deltas equal the non-stream answer over the same fields | pass, 17 chunks |
| chat stream finish | last chunk carries `finish_reason` `length` | pass |
| chat with `stop: [" "]` | `finish_reason` `stop` | pass |
| usage | prompt/completion/total present and `total == prompt + completion` | pass, `13 + 16 = 29` |
| malformed chat body (`messages` a string) | HTTP 4xx | pass, `400` |

## Finding: this model answers in `reasoning_content` at small budgets

The first run failed the body check because `content` was empty. The response was:

    "message": { "role": "assistant", "content": "", "reasoning_content": "The user just wrote ..." }

With `max_tokens: 16` the whole budget went into the model's reasoning field, so an empty `content` is the model's
correct behaviour, not an endpoint defect. The suite now concatenates `content` and `reasoning_content` on both the
non-stream and the stream path, which also makes the stream-equality check meaningful for reasoning models.

## Coverage boundary

Still not covered: the responses API, tool calls (this profile does not emit them), token-usage details beyond the three
totals, and multi-client interleaving. `prompt_tokens_details.cached_tokens` is present and zero, which matches the
accepted profile's `--cache-ram 0`.
