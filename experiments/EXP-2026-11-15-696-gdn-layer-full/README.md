# EXP-2026-11-15-696 — the recurrent layer is complete and bit-identical

Base: `4a1b8e03`. No model run, no GPU.

## 1. Result

```
cmp alpha    n=32     differs=0      cmp gdn   n=528384 differs=0
cmp beta_sig n=32     differs=0      cmp core  n=4096   differs=0
cmp gate     n=32     differs=0      cmp z     n=4096   differs=0
cmp conv     n=8192   differs=0      cmp norm  n=4096   differs=0
cmp silu     n=8192   differs=0      cmp out   n=2048   differs=0
cmp q_view   n=2048   differs=0
cmp k_view   n=2048   differs=0
cmp v_view   n=4096   differs=0
FG_GDN_LAYER_FULL ... bad=0 PASS
```

Thirteen nodes covering the whole layer, not just the chain EXP-690 proved:

```
beta  = sigmoid(reshape(w_beta * cur, [1, n_v, T, B]))
alpha = reshape(w_alpha * cur, [n_v, T, B])
gate  = reshape(softplus(alpha + ssm_dt) * ssm_a, [1, n_v, T, B])
conv  = silu(ssm_conv(conv_input, kernel))
q/k/v = views at the qkv stride; l2_norm(q), l2_norm(k); repeat to n_v
core  = gated_delta_net(q, k, v, gate, beta, state, K)
out   = view_4d(core, head_v, n_v, T, B)
norm  = rms_norm(out) * silu(reshape(z, [head_v, n_v, T, B]))
cur'  = ssm_out * reshape(norm, [head_v*n_v, T, B])
```

## 2. The two defects EXP-695 recorded, and their fixes

**`gated_delta_net` returns output and state together.** `goml.c:6333` builds
`ne = { S_v*H, n_tokens*n_seqs + K*S_v, 1, 1 }`. With K=1 and one token that is
`[4096, 129]`. The fix is to slice:

```c
core = ggml_view_4d(gdn, head_v, n_v, T, B,
                    elem*head_v, elem*head_v*n_v, elem*head_v*n_v*rows, 0);
```

`cmp core n=4096 differs=0` is that slice, compared against the native one. Without it
the gated norm aborted in `ggml_mul`, because a `[4096,129]` tensor cannot multiply a
`[128,32]` gate.

**The previous `gdn` comparison was over 528 384 elements** - the whole
output-plus-state buffer. It passed while the layer was wrong. Now `gdn` still compares
over 528 384 and `core` separately over 4096, so both the full node and the slice are
checked.

## 3. Why this attempt succeeded where the last failed

EXP-695's attempt assembled the packed argument array by hand for every call and
computed the alpha branch twice. This one uses the same `call(mode, gc, a, b, d, op,
inplace, a7..a14)` helper as the working attention gate, with named parameters, and
prints the mode on every call while debugging.

Three real mistakes surfaced that way, none of them in the model:

1. mode 11 needs `GGML_OP_MUL_MAT` in the op slot; passing 0 made `common()` build a
   node with no operation.
2. mode 4 needs 0 in the inplace slot before `eps` in a7; sending eps in the inplace
   slot made `ensure_g` abort.
3. the Goose arm projected `nrmA` without first reshaping it to `[head_v*n_v, T, B]`.
   `wout->ne[0]` is 4096 and `nrmA->ne[0]` is 128, so `can_mul_mat` refused. The native
   arm did reshape - a genuine asymmetry between the two arms, which is exactly what a
   differential gate is for.

## 4. State of the recurrent path

| piece | state |
|---|---|
| beta, gate branches | bit-identical (this card) |
| conv, q/k/v views, l2_norm, repeat | bit-identical (EXP-690/691, rechecked here) |
| gated_delta_net incl. state slice | bit-identical (this card) |
| gated norm, ssm_out projection | bit-identical (this card) |
| state carried BETWEEN calls | not built |

## 5. Cost

Gate compiles only. No unit rebuild.
