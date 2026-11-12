# EXP361: context boundaries on the matched candidate

Three shapes against a 256-token context.

| case | prompt | n_predict | native | candidate |
|---|---|---|---|---|
| near capacity | ~80 tokens | 8 | `8 tokens, limit` | identical |
| over capacity | ~240 tokens in a 256 context with generation | 8 | **HTTP 400** | identical |
| long generation | 2 tokens | 512 | stops at 253 tokens, `limit` | identical |

The first attempt used a ~200-token prompt, which the server rejected with 400 on both lines before doing any work -- a correct but uninformative
result, so the prompt was shortened until the near-capacity path actually ran. The record keeps both versions because "both rejected it identically"
is a smaller claim than "both generated identically".

The long-generation case is worth noting: both lines stop at **253** tokens rather than 256, so the effective room is computed the same way at the
boundary.

## Scope

One model, one context size, no eviction beyond the boundary. Slot shifting and the near-capacity reuse rules that P5-07 asks about are not
exercised here.
