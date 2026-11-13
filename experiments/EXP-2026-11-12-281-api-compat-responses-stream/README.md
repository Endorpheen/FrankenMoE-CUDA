# EXP281: streaming /v1/responses -- P8-04 closes

The last shape the criterion names. One server, deterministic request (`temperature: 0`, `max_output_tokens: 48`), run
non-stream and stream.

## Results

| check | result |
|---|---|
| non-stream aggregate | text 11 chars, reasoning 74 chars, items `['reasoning', 'message']` |
| stream event names | 32 events starting `response.created`, `response.in_progress`, `response.output_item.added` |
| stream termination | `response.completed`, status `completed` |
| stream usage | input 16 / output 27 / total 43, `cached_tokens = 12` |
| streamed text | 11 chars, equal to the non-stream text |
| streamed reasoning | 74 chars, equal to the non-stream reasoning |

`cached_tokens = 12` is worth noting: the second request reuses the first one's prefix through the accepted runtime's
context checkpoint path, which is the mechanism EXP268/269 made Goose-owned -- the counter behaves.

## P8-04 closure

The criterion asks for a native API compatibility suite covering stream and non-stream completion, chat and responses,
token usage, finish reasons, errors and cancellation. Across EXP278-281 every one of those is checked on the accepted
runtime:

| area | evidence |
|---|---|
| completion, non-stream and stream | EXP278: baseline `e68df05b`, 128 tokens, stream equal over 129 chunks |
| finish reasons | EXP278 (`limit`, `word`), EXP279 (`length`, `stop`) |
| chat, non-stream and stream | EXP279: 17 chunks equal, reasoning-content handled |
| usage | EXP279 `13+16=29`, EXP280 `15+20=35`, EXP281 `16+27=43` |
| errors | EXP278 completion 400, EXP279 chat 400 |
| cancellation | EXP278 disconnect then served; EXP267 shutdown mid-stream |
| responses, non-stream and stream | EXP280, EXP281 |

Not in the criterion and therefore not claimed: tool calls (this profile emits none) and multi-client queue fairness
beyond the two-client check in EXP280.
