# EXP-2026-11-15-685 — a three-node Apex layer prefix, built by Goose and executed

Base: `a668f22e`. No model was started, no GPU was touched.

## 1. What runs

The Goose graph builder emits a three-node prefix of an Apex layer at the model's
own extents, and the backend executes it:

```
FG_APEX_GRAPH nodes=3 K=2048 N=8192 declared_ok=1 shape_ok=1 status=0
              first=-0.167770 want=-0.167770 rel=1.46e-08 PASS
```

| node | mode | op | source |
|---|---|---|---|
| rms_norm on the activation | 4 | `GGML_OP_RMS_NORM` | module validates padded-1d, eps in param 0 |
| projection against the real weight | 11 | `GGML_OP_MUL_MAT` | module validates `can_mul_mat`, derives the result extent |
| silu on the projection | 3 | unary `GGML_UNARY_OP_SILU` | module validates contiguous rows |

`K=2048, N=8192` are read from `blk.0.attn_qkv.weight` of the real 35B-A3B file;
8192 is the fused query/key/value width the architecture builder computes and the
gate checks that both agree (`declared_ok=1`).

## 2. The check that makes it meaningful

The host computes the same prefix independently: the RMS over 2048 values, one dot
product per output column, then `x / (1 + exp(-x))` on the first result. The two
agree to `1.46e-08`, which is float rounding, not a coincidence of shared code.

Nothing about the geometry is decided by the test: mode 4 requires padded-1d, mode
11 requires `can_mul_mat` and derives the output extent, mode 3 requires contiguous
rows. A wrong shape stops at the builder with a distinct code instead of producing
a silently wrong tensor.

## 3. Why this is the layer skeleton

The three nodes are the first three of every Apex layer: normalise the residual,
project to the fused qkv width, gate the result. The remaining nodes of a layer
follow the same pattern - each is one `gb_common` mode with the module deciding
shape legality.

What is still missing for a full layer: rope and attention (both ops exist in the
module as `op_rope` and `op_flash_attn_ext`), the output projection, and for the
31 recurrent layers the whole GDN stack (`op_ssm_conv`, `op_gated_delta_net`).
Then KV buffers, CUDA, token generation and the model run.

## 4. Cost

One gate compile, no unit rebuild, one run.

## 5. Reproduce

```
./builds/goose-standalone/apex_graph <apex-model.gguf>
```
