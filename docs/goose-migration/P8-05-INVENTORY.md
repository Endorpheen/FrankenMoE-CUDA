# P8-05 inventory: embedding, reranking, tokenization and utility endpoints

Probed live on the accepted runtime (EXP-282) rather than read off the route table.

## What serves this model

| endpoint | result | what it means |
|---|---|---|
| `POST /tokenize` | 200, `"Hello world"` -> `[9419, 1814]` | live; the tokenizer core behind it is Goose-owned (APEX.TOKENIZER: BPE merge control EXP-202, codepoint decode EXP-215, detokenisation EXP-213P/401) |
| `POST /detokenize` | 200, `[9419, 1814]` -> `"Hello world"` | live, exact round trip |
| `POST /apply-template` | 200, ChatML (`<\|im_start\|>user ... <\|im_end\|> ...`) | live; the decisions are the Jinja template engine's, a third-party library |
| `POST /infill` | 200 | served, but this model is not a FIM model, so the answer is ordinary completion; the FIM branch (prefix/suffix tokens) is not exercised |
| `GET /props` | 200, `n_ctx = 262144`, chat template present | live, serialisation only |
| `POST /embeddings` | **501** "does not support embeddings. Start it with `--embeddings`" | flag off and the model has no pooling head |
| `POST /rerank` | **501** "does not support reranking. Start it with `--reranking`" | same |

## What that means for the item

- Tokenization, the only piece the Apex profile actually uses, is already Goose-owned at the decision level; the endpoint
  adds JSON marshalling around it.
- Embeddings and reranking cannot be exercised here: they need a different model (pooling or a reranker) and a flag the
  accepted profile does not set. Porting their kernels is possible, but it would be dormant work of the same kind as the
  Q5_K loader, and the item's acceptance would still be unprovable on this profile.
- `apply-template` is a template-engine surface; the engine is vendored third-party code, so "porting" it is a decision
  about whether to own a Jinja implementation, not a routine port.

## What would close it

1. A decision on scope: tokenization endpoints (live, already Goosed) versus embeddings/rerank (need another model) versus
   the Jinja engine (vendored).
2. If embeddings/rerank are in scope, an embedding model and the corresponding flag in an agreed configuration, plus its
   own compatibility checks.

Nothing here is claimed as ported beyond the tokenizer core that already exists.
