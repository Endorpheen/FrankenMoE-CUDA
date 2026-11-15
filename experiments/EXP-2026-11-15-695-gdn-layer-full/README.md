# EXP-2026-11-15-695 — the recurrent layer's full sequence: two real defects found

Base: `8e135b29`. No model run, no GPU. **No passing gate is claimed here.** This card
records two defects the extension of the layer gate exposed, and why the earlier PASS
did not cover them.

## 1. What EXP-690 and EXP-691 actually proved

They proved the chain

```
conv -> silu -> view q/k/v -> l2_norm -> repeat -> gated_delta_net
```

against ggml's constructors, bit for bit. They did **not** cover:

- `beta = sigmoid(reshape(w_beta * cur, [1, n_v, T, B]))`  (qwen35moe.cpp:386-391)
- `gate = softplus(alpha + ssm_dt) * ssm_a`  (qwen35moe.cpp:393-406)
- the gated norm `rms_norm(core) * silu(z)`  (qwen35moe.cpp:474-486)
- the `ssm_out` projection and the `z` branch

In the gate, `beta` and `gate` were synthetic tensors of the right shape. So the shape
was checked and the arithmetic was not.

## 2. Defect 1: `gated_delta_net` returns the output AND the next state

From `ggml.c:6320-6335`:

```c
const int64_t ne[4] = { S_v * H, n_tokens * n_seqs + state_rows, 1, 1 };
```

with `state_rows = K * S_v * n_seqs`. The node is not an elementwise-producing op: its
result carries the next state appended below the output. With `K=1`, one token and one
sequence the result is `[4096, 129]`, not `[128, 32]`.

The native code slices it (`delta-net-base.cpp:577-600`): `view_4d` for the output and
`view_3d` for the state. Feeding the whole tensor onward aborted in `ggml_mul` with
`GGML_ASSERT(ggml_can_repeat(b, a))` - because a `[4096, 129]` tensor cannot multiply a
`[128, 32]` gate.

This is worth stating plainly: EXP-690 compared `gdn` over 528 384 elements and got
zero differences. 528 384 is `4096 * 129` - the whole output-plus-state buffer. Both
arms built the same *wrong* tensor, so the comparison was true and the layer was not.

## 3. Defect 2: the state is an output, not a side effect

The same result shape means the recurrent layer does not need `cpy` at all for the SSM
state: the next state is a slice of the node's output. The conv state still needs the
tail-and-copy pattern of `build_conv_state`, but the SSM state is returned. The
`STATE-BUFFERS.md` design assumed both were writes; that assumption is corrected here.

## 4. Where the extension stands

The gate was extended to the full sequence (beta, gate, gated norm, output projection)
and is **not passing**. It aborts while constructing the gate branch a second time for
the native arm, and the abort is not yet explained. The file is reverted to the
committed version rather than left failing in the tree, and the honest state is: the
recurrent chain through `gated_delta_net` is proven; the layer is not.

## 5. What the next attempt must do first

Restate the native sequence line by line - as the lesson from EXP-694 says - and then
build the graph in that order with **no duplicated expressions between the arms**. The
attempt above computed the alpha branch twice (once for the Goose arm, once for the
native arm) and the second copy aborted; a single expression per node, with the arms
differing only in which constructor builds it, removes that class of failure entirely.
