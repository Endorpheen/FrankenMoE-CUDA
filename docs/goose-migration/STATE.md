# State between calls — broken into closed units

"State between calls" is not one blocker. It is eight, and each has its own evidence and
its own commit. This file is the ledger for them; `tasks.json` mirrors the counts.

Status legend: **DONE** has a passing check and a commit. **OPEN** has a named next step.

| # | unit | status | evidence |
|---|---|---|---|
| 1 | attention KV slot arithmetic | DONE | EXP-701, `attn_kv_slot` in Goose, 3 tokens bit-identical to a rebuild |
| 2 | attention KV write/keep/read | DONE | EXP-701, same check: positions persist across tokens |
| 3 | recurrent conv state write/read | DONE | EXP-692, `gdn_conv_input`/`tail`/`slot`/`store`, 2 tokens, state_nonzero=10120 |
| 4 | recurrent SSM state as an output | DONE | EXP-696, `view_4d` of the `gated_delta_net` result, `core` 4096 elements |
| 5 | one mechanism for both caches | OPEN | KV uses `attn_kv_slot`, recurrent uses `gdn_conv_slot`; they should be one allocator with two layouts |
| 6 | buffer survives a graph rebuild | OPEN | every gate allocates its own context; nothing yet hands a buffer between two `ggml_init` calls |
| 7 | reuse: skip what is already computed | OPEN | needs the position cursor and the `n_past` boundary |
| 8 | comparison against a full recompute | DONE for KV and conv | EXP-701 (KV), EXP-692 (conv); not yet for the SSM state across calls |

## What each OPEN unit needs, concretely

**5 — one mechanism.** Both slots answer the same question: "give me the view of this
layer's state for these positions". `attn_kv_slot` takes `n_kv_heads, n_ctx`; `gdn_conv_slot`
takes `kernel-1, channels` and a flat `mem_size`. One export that takes a layout descriptor
and returns the view would remove the second copy. Next step: write
`state_slot(layout, layer, pos, ...)` in Goose with both layouts as cases, and re-point the
two gates at it.

**6 — surviving a graph rebuild.** A buffer has to be allocated outside any graph context
and handed in. This is runtime structure, not a decision, so it stays in the host - but the
offset arithmetic stays in Goose. Next step: a small `state_alloc`/`state_attach` pair that
owns one `ggml_context` for the caches and passes tensor handles into per-token contexts.

**7 — reuse.** The cursor is `n_past`: positions below it are already in the cache and must
not be recomputed, positions from it onward must be. The correctness statement is the one
already used: cached results equal a full recompute. Next step: a two-call gate where the
first call writes positions 0..1 and the second writes 2..3 and reads all four, compared
against a single four-position pass.

## The measure this replaces

Before this split the phrase "state between calls" covered four finished checks and four
open ones, and reporting it as one blocker understated what was done and hid what was left.
Each row above is now either a commit or a named next action.
