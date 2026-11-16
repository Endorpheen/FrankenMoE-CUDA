# EXP-2026-11-17-889 — the whole can_reuse family moved to Goose in one batch

Igor's order: port in the largest safe batches, whole subsystems, with cheap local checks during the batch and
full model checks once the batch is finished. This is the first batch done that way: 14 functions at once.

## Result

```
decision coverage   4/49 (8.2%)  ->  18/49 (36.7%)
512 tokens, module ON :  sha c2ff42cc1219   decode 42.42 (42.03-42.44)
128 tokens, module OFF:  sha 9d90a010775f   (= reference for 128)
```

Both paths match the reference hashes, so the switch is a working rollback and not a second code path.
42.42 tok/s is the best decode seen in this session; the honest reading is that the reuse decision got cheaper
and the machine was also favourable, so it is recorded as "not slower, possibly faster" rather than claimed as a
verified speedup without a same-config control from the same hour.

## What moved

Every remaining `llm_graph_input_*::can_reuse` conjunction, 85 native `res &= ...` lines across 14 functions:
`pos`, `out_ids`, `rs`, `attn_kv`, `attn_k::can_reuse_impl`, `attn_kv_msa`, `attn_k_dsa::can_reuse_impl`,
`attn_k_dsa_iswa`, `attn_kv_iswa`, `attn_k_iswa`, `dsv4`, `mem_hybrid`, `mem_hybrid_k`, `mem_hybrid_iswa`.

Each native line became one row of the Goose decision. Lines that call a sub-decision (`can_reuse_kq_mask`,
`dsv4_can_reuse_comp_input`, `can_reuse_impl`) stay native and enter as a 1/0 row - the change is where the
conjunction is decided, not what is decided.

## How the batch was made safe, after two failed attempts

Two earlier attempts were reverted. Both would have compiled. Both were wrong:

1. **brace counting** - produced a file where per-function row counts no longer matched the scan, the signature
   of a body spliced into the wrong function;
2. **`\n}\n` search** - the boundary landed past the function: `llm_graph_input_embd::can_reuse` measured 2190
   bytes and contained two `fg_gr_check` calls because it had swallowed `embd_h`.

The failure that finally blocked both was a self-inflicted counting error, and it is worth recording because it
fooled the checker twice: **the comment inserted into each function contained the very literal the post-check
counted** - first `res &= `, then `fg_gr_check`. The checker was right to refuse; the check was measuring its own
insertion.

What worked was lines instead of substrings: a body runs from its signature to the first line that is exactly `}`
in column 0, the row count must be identical before and after, exactly one `return fg_gr_check(` must appear, and
the file's total conjunction count must be unchanged. Anything else aborts before writing.

## Why a bad batch here would have been invisible

`can_reuse` decides whether a graph can serve the incoming batch. Only one architecture's path runs in any given
test, and the reference hash covers that one. A body spliced into the wrong function still compiles, still keeps
the Apex hash, and silently breaks the twelve architectures that are not being executed. That is why the batch
needed a mechanical invariant rather than a careful reading.

## Cost

Two builds, two Apex runs (3 repeats each) for the whole batch - not one per function.
