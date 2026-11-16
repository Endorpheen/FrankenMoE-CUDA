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
