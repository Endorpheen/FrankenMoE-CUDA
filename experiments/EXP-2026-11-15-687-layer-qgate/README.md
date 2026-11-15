# EXP-2026-11-15-687 — the layer needs a q-gate split: what blocks the full-layer graph

Base: `1907e19a`. No model run, no GPU.

An attempt to wire one attention layer into a single 13-node graph stopped at a
real architectural detail. This card records it, because the detail is the
difference between "the ops work" and "the layer works".

## 1. The extents, from the model

```
blk.3.attn_q.weight         ne=[2048 8192 1 1]
blk.3.attn_k.weight         ne=[2048  512 1 1]
blk.3.attn_v.weight         ne=[2048  512 1 1]
blk.3.attn_output.weight    ne=[4096 2048 1 1]
```

With `head_count=16` and `head_count_kv=2`, this means:

| tensor | per-head width | total |
|---|---:|---:|
| q | 512 | 8192 |
| k | 256 | 512 |
| v | 256 | 512 |
| attention out (input) | 256 | 4096 |

## 2. Why that stops a naive chain

`flash_attn_ext` requires `can_mul_mat(k, q)`, which starts with
`k->ne[0] == q->ne[0]`. Here they are 256 and 512. The layer, therefore, cannot
feed the q projection straight into attention: **q carries twice the attention
width because half of it is a gate**, exactly as the architecture builder's
`T_WQKV_GATE` says for the recurrent path and `attn_gate.weight` says for the
attention path - the model file has `blk.0.attn_gate.weight` alongside
`blk.0.attn_qkv.weight`.

So the layer is:

```
x -> rms_norm -> wq            -> split -> q_attn [256,16], gate [256,16]
              -> wk            -> k       [256,2]
              -> wv            -> v       [256,2]
q_attn -> rope, k -> rope -> flash_attn(q, k, v) -> [256,16] = 4096
gate   -> silu  (applied to the attention output, not before)
attn * silu(gate) -> wo -> residual
```

The split and the gate application are two nodes the chain does not have yet, and
a view with an offset is what the module's modes 23/24 provide (`cont` over a
slice). Building that is the next step; guessing the order would produce a graph
that runs and computes the wrong thing.

## 3. What is still proven

Each node individually, bit-exact against the substrate, at real Apex extents:

| node | check |
|---|---|
| rms_norm + qkv + silu + out proj + residual | 5-node block, 8.07e-08 vs host |
| rope (q and k widths) | bit-exact vs `ggml_rope_ext`, positions 0 and 1 |
| flash_attn | bit-exact vs `ggml_flash_attn_ext`, 2048 elements |
| MoE expert matmul | exact, 256 experts / 8 used |
| GDN conv | bit-exact vs `ggml_ssm_conv` |
| gated_delta_net | bit-exact vs `ggml_gated_delta_net`, 264 192 elements |

What is not proven is the wiring of a whole layer, and the blocker is now
specific: the q-gate split, then the gate application.

## 4. Correction to an earlier assumption

The five-node block used `head_dim = K/H = 128` and was therefore a projection
round-trip, not this model's attention geometry. It remains a valid check of those
five nodes at real embedding width, but it must not be read as "the attention block
of this model". The real head widths are 512 for q and 256 for k/v, and any layer
graph must use those.

## 5. Cost

Gate compiles only; the unit was not rebuilt. The failing attempt was removed
rather than left in the tree, and the reason is recorded here.
