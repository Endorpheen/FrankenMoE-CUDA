# EXP-2026-11-15-690 — the recurrent layer's chain is bit-identical to a native build

Base: `71f085df`. No model run, no GPU.

## 1. Result

```
cmp conv    n=8192   differs=0     cmp q_norm  n=2048 differs=0
cmp silu    n=8192   differs=0     cmp k_norm  n=2048 differs=0
cmp q_view  n=2048   differs=0     cmp q_rep   n=4096 differs=0
cmp k_view  n=2048   differs=0     cmp k_rep   n=4096 differs=0
cmp v_view  n=4096   differs=0     cmp gdn     n=528384 differs=0

FG_GDN_LAYER kernel=4 chan=8192 head_k=128 n_k=16 head_v=128 n_v=32 bad_nodes=0 PASS
```

Ten nodes, every one bit-identical to the graph a native builder would emit:

```
conv  -> silu -> view q/k/v (qkv stride) -> l2_norm q,k -> repeat to v heads
      -> gated_delta_net
```

This is the recurrent half of the layer - the path 31 of the 42 Apex layers take. The
norm/projection/residual half is the same as the attention layer, proven in EXP-688b.

## 2. The dimensions, from the weights

| tensor | extent | meaning |
|---|---|---|
| `blk.0.ssm_conv1d.weight` | [4, 8192] | kernel 4, 8192 conv channels |
| `blk.0.ssm_norm.weight` | [128, 1] | value head width 128 |
| `blk.0.ssm_out.weight` | [4096, 2048] | value dim 4096 -> 32 value heads |
| `blk.0.attn_qkv.weight` | [2048, 8192] | 2*head_k*n_k + head_v*n_v |

From those: `head_k=128`, `n_k=16`, `head_v=128`, `n_v=32`. The repeat arm therefore
runs (32 % 16 == 0), which is why it is exercised rather than skipped.

## 3. The stride decision moved into Goose

The first version of this gate carried the three `view_4d` stride numbers itself -
the same debt the QG split had before EXP-687c. That debt is now closed:
`gdn_qkv_view` in `goose/model/gdn-ops.goose` computes

    nb1 = elem * (2*head_k*n_k + head_v*n_v)

and the three offsets, and the gate only names which view it wants. One symbol with
a `which` selector rather than an array out-parameter, so the C side calls it with
plain integers and the decision never leaves Goose.

The evidence file `evidence-691.txt` is the same gate rerun after the move: still
`differs=0` at all ten nodes, with the stride arithmetic now on the module side.

The rest of the chain goes through Goose: conv and gated_delta_net are `gb_common`
modes 27 and 28, l2_norm and repeat are `gdn-ops` exports.

## 4. State

| piece | state |
|---|---|
| attention layer, 15 nodes | bit-identical to a native build (EXP-688b) |
| recurrent layer chain, 10 nodes | bit-identical to a native build (this card) |
| conv/ssm state buffers | **not built** - the recurrent layer's KV cache |
| 42-layer stack, prefill, decode, CUDA, tokenizer, HTTP | not built |

The two layer kinds are now proven at the node and chain level. What remains before
a real inference is the state plumbing and the stack, not the arithmetic.

## 5. Cost

Gate compile only; no unit rebuild.
