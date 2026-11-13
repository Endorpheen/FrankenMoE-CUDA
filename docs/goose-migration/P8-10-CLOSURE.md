# P8-10 closure: Apex server/API acceptance corpus

Criterion: "All audited route/method/schema combinations have tests; Apex real
end-to-end request, stream, cancel, reuse; Qwen major milestone."

Qwen is assigned to C1-05 in stage 11 and is not claimed here. This note closes
the Apex half of the corpus.

## Audited routes and methods

| route | method | cases | evidence |
|---|---|---|---|
| `/health` | GET | ready/ok, JSON | EXP281/EXP282, EXP424 |
| `/props` | GET | schema, n_ctx, template, params | EXP282, EXP350 |
| `/completion` | POST | non-stream, stream, stop word, limit, 400 | EXP278/EXP424 |
| `/v1/chat/completions` | POST | non-stream, stream deltas, finish_reason, usage | EXP279 |
| `/v1/responses` | POST | completed, usage, max_output_tokens | EXP280 |
| `/tokenize` | POST | token bytes `[9419,1814]` | EXP282 |
| `/detokenize` | POST | exact round trip | EXP282 |
| `/apply-template` | POST | default/custom templates, kwargs | EXP352 |
| `/infill` | POST | serves; model is not FIM, branch recorded | EXP282 |
| `/slots` | GET | 501 by flag in the agreed profile | EXP282/EXP288 |
| `/metrics` | GET | 501 by flag in the agreed profile | EXP282/EXP288 |
| `/embeddings` | POST | 501 by flag/model contract | EXP282 |
| `/rerank` | POST | 501 by flag/model contract | EXP282 |

The 501 routes are not untested code paths: their status is the registered
contract for the agreed profile, and the owner scope decision for P8-05/P8-08
records that.

## Apex end-to-end

| clause | evidence |
|---|---|
| request | EXP278 non-stream `e68df05b`, 128 tokens, finish `limit` |
| stream | EXP278 129 chunks, equal content and token ids |
| cancel | EXP379 disconnect; EXP424 disconnect and next request exact |
| reuse | EXP361 context boundaries, EXP308 rewind, EXP418 near-capacity save/restore |
| concurrency | EXP424 four clients, invalid JSON 400, second lifecycle |
| response API | EXP280 completed response, usage, output budget |
| chat API | EXP279 finish_reason mapping, usage, stream/non-stream equality |

## Qwen

Not measured. Assigned to **C1-05** (qualify complete Qwen inference in the
common artifact) and C1-06 (protect Apex). The routes are the same shared server
code; the model-specific milestone is a separate compatibility task.

## Verdict

P8-10 closes on the Apex corpus. The route table is the audited set; routes that
return 501 are explicit status contracts, not silent omissions. Qwen stays
visible in C1-05 and is not counted as tested.
