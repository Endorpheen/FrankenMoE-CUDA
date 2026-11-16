# Reachability witness: only three of the ported reuse variants actually run, and EXP-893 was wrong

EXP-893 inferred from "hybrid memory appears in the log" that roughly 12 of the 21 ported `can_reuse` functions
were exercised, and that the reference hash was therefore evidence for them. That inference was too generous, and
this measures the thing directly instead of reasoning from a nearby signal.

## The witness

`fg_gr_check` now announces each distinct row count once. The count identifies the caller closely enough: 5 is
`embd`, 6 is `mem_hybrid`, 3 is `embd_h` or `attn_kv_msa`, 2 is `attn_kv` or `attn_k`, 4 is `rs` or `attn_k_iswa`,
8 is `mem_hybrid_iswa`, 1 is `pos`, `out_ids` or `attn_temp`.

One 128-token run of the pinned profile:

```
FG_GR_REUSE rows=5 first_call calls=1
FG_GR_REUSE rows=6 first_call calls=2
FG_GR_REUSE rows=1 first_call calls=3
```

and the content hash is `9d90a010775f`, which is the reference for 128.

## What that means

| rows | caller | appeared |
|---|---|---|
| 5 | `llm_graph_input_embd` | yes |
| 6 | `llm_graph_input_mem_hybrid` | yes |
| 1 | `pos` / `out_ids` / `attn_temp` | yes |
| 3 | `embd_h` / `attn_kv_msa` | **no** |
| 2 | `attn_kv` / `attn_k` | **no** |
| 4 | `rs` / `attn_k_iswa` | **no** |
| 8 | `mem_hybrid_iswa` | **no** |

So the honest statement is: the pinned profile reaches **three** of the ported variants, not twelve. Everything
the reference hash can vouch for is `embd`, `mem_hybrid`, and one single-row function. `rs`, `attn_kv`, `attn_k`,
`attn_kv_msa`, `embd_h` and all the ISWA/DSA variants are ported but unexercised.

That also means `embd_h`, whose native body I deleted in `d1def8bc`, was **not** exercised before deletion. It was
proven equal by EXP-886's 4000-case differential gate, which is a stronger proof of the decision than a hash would
have been - but the commit message said "both paths produced the reference hash", and for `embd_h` that was not
true. Correcting that here rather than leaving the claim standing.

## Correction to EXP-893

EXP-893 read `FG375S_HYBRID_MEMORY calls=100` as evidence that the hybrid input's siblings were exercised too. The
witness shows that hybrid memory is reached while `rs` (its recurrent-state sibling, 4 rows) is not, and that the
ISWA variants are not. Proximity in the architecture is not reachability of a call.

## Why this matters more than the count

The remaining native bodies are kept for exactly these unexercised functions. Before this measurement the reason
was a reasoned guess; now it is a list. Removing their C++ needs a configuration or harness that produces
`rows=2`, `rows=3`, `rows=4` or `rows=8` in a log - a condition that can be checked in one line instead of argued.

## Why rows=2/3/4/8 cannot be reached with what is on disk

The remaining variants belong to memory and attention configurations that this machine has no model for:

- `rows=2` is `attn_kv`/`attn_k`, plain attention-only memory. Apex is hybrid, so it never builds that input;
- `rows=3` is `attn_kv_msa` (multi-stream attention, DeepSeek-style) or `embd_h` (needs a state embedding in the
  batch, which the pinned request does not send);
- `rows=4` is `rs` (recurrent-only context, as opposed to the hybrid one Apex uses) or `attn_k_iswa` (needs
  sliding-window layers, which Apex does not have);
- `rows=8` is `mem_hybrid_iswa`, hybrid **with** SWA layers.

Only two models exist here: Apex (hybrid, no SWA) and flint. Neither has SWA layers, and neither is
attention-only. So no flag on the pinned profile can produce those counts - it is a property of the model, not of
the configuration.

A harness that constructs the inputs directly would need a real `llama_kv_cache_context` (for `get_base()`), a
real recurrent memory context (for `get_n_rs()`, `get_head()`, `get_rs_z()`) and SWA layers for the iswa variants.
That is a model, not a stub, so the harness would end up testing a fake of the thing it is supposed to verify.

## Consequence, stated plainly

The five unexercised functions keep their native bodies, and that is not a temporary inconvenience to be worked
around - it is the correct state until a model with the matching memory configuration is available. Their Goose
ports are written, compile, and are proven equal by construction to the native expressions (same guards, same
order, same sub-decisions), but they are not verified against a run, and the difference between "ported" and
"verified" is the whole subject of EXP-892.

The reachability witness stays in the bridge permanently. When a model with SWA or attention-only memory is added,
one run prints `rows=2`, `rows=3`, `rows=4` or `rows=8` and the question is answered without any new tooling.
