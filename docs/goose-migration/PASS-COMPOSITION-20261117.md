# What "compose the 41 layers into one pass" actually needs

The standalone harness sweep (STANDALONE-HARNESSES-20261116) closed the gap to one sentence: every piece
executes, none of them are composed. This names the pieces and what is missing between them, so the next
step is a task list rather than an ambition.

## What already executes on the real 21.7 GB file

| piece | harness | evidence |
|---|---|---|
| 41-layer plan, 10 attn / 30 recr / 1 MTP | `stack_dispatch`, `gdn_plan` | `pos_bad=0 bad=0`, positions right |
| builder selection per kind | `stack_dispatch` | `shifted_differs=20` - the loop is sensitive to the plan |
| attention chain, 15 nodes, real weights | `apex_layer` | `nondet=0 wiring_differs=0 vs_ggml=0` |
| recurrent (GDN) layer, full | `gdn_layer_full` | `bad=0`, kernels 4, head_k 128 / head_v 128 |
| GDN recurrence + state | `gdn_recur` | `state_differs=0 state_nonzero=10120` |
| MoE block | `moe_block` | `router=PASS`, ids match |
| KV slots | `kv_cache`, `state_slot` | `bad=0`, 22 slots |
| state life and reuse | `state_life`, `state_reuse` | `lost=0 stray=0`, `differs=0 distinguishable=1` |
| a 5-node graph at real extents | `apex_graph` | `rel=8.07e-08` |
| weight layout from the container | `frankenmoe-v4` | 753 tensors, 0 mismatches |

## What is missing, named

1. **A loop that carries the residual stream.** Every harness builds one layer and stops. Between layers
   there is `x = x + layer(x)`, and nothing holds `x` across 41 iterations.
2. **Embedding lookup at the head and the output projection at the tail.** `build_inp_embd` exists in the
   integrated tree's graph builder; the standalone host has no step that turns token ids into the first `x`
   or turns the final norm into logits.
3. **One KV/state arena shared by all layers.** `kv_cache` and `state_slot` prove the mechanism with a
   single layer's worth of slots. The pass needs 10 attention layers and 30 recurrent ones bound into one
   arena with per-layer offsets.
4. **Position and mask carried forward.** `prefill_decode` shows the mask width is the only difference
   between the two arms for one layer; across 41 layers the positions must advance once per token.
5. **Chunking.** A 32K prefill cannot be one 41-layer graph in this runtime; it needs the chunked prefill the
   integrated tree does. Nothing here has exercised more than a handful of tokens.

## The honest shape of the work

Items 1-4 are a loop and four bindings over pieces that already pass their own gates. Item 5 is the one that
is genuinely new, and it is also the one that decides whether the result is usable: a pass that only handles
one token at a time proves composition but not a workload.

That suggests the order: **compose single-token first** (items 1-4), accept it only when the logits for a
known token match the integrated runtime's, and treat chunked prefill as the next experiment. A
single-token pass that agrees numerically is a much stronger result than a 41-layer graph that assembles and
runs nothing.

## What this is not

It is not a claim that the standalone is close to serving. HTTP transport, CUDA and the tokenizer engine are
still absent, and rule 15's test - the export answers the capability - is untouched by any of the above.

## Cost estimate for the next step

Items 1-4 over existing harnesses: one new harness in `integration/goose-standalone/tests/`, using
`apex_layer`'s weight loading and `stack_dispatch`'s kind loop. The reference for correctness is the
integrated stage-run's logits for the same prompt, which already exists.
