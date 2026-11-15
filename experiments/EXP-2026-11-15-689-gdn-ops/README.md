# EXP-2026-11-15-689 — the two missing GDN operations, proven against ggml

Base: `355ab834`. No model run, no GPU.

## 1. What was missing and why

The recurrent layer (31 of the 42 Apex layers) needs two graph operations the
builder had no mode for, read from `qwen35moe.cpp:459-470`:

```c
q_conv = ggml_l2_norm(ctx0, q_conv, eps_norm);
k_conv = ggml_l2_norm(ctx0, k_conv, eps_norm);
if (num_k_heads != num_v_heads) {
    q_conv = ggml_repeat_4d(ctx0, q_conv, head_k_dim, num_v_heads, n_seq_tokens, n_seqs);
    k_conv = ggml_repeat_4d(ctx0, k_conv, head_k_dim, num_v_heads, n_seq_tokens, n_seqs);
}
```

`goose/model/gdn-ops.goose` owns them: `gdn_l2norm` passes the epsilon as f32 bits
rather than reinterpreting a float through an integer parameter, and
`gdn_repeat_to_v` carries the native guard (the value head count must be a multiple
of the key head count) and returns the input untouched when the counts already
match, which is what the native builder does by skipping the op.

Two primitives back them, `fg158_l2norm` and `fg158_repeat4`, and both are
translation only.

## 2. Result

```
FG_GDN_OPS l2norm elements=256 eps=1e-06 differs=0
FG_GDN_OPS repeat elements=512 n_k=2 n_v=4 differs=0
FG_GDN_OPS skip_when_equal=1 PASS
```

Three things are checked, and the third is the one a naive implementation would
miss:

1. `l2_norm` and `ggml_l2_norm` agree bit for bit over 256 elements.
2. `repeat_4d` and `ggml_repeat_4d` agree bit for bit over 512 elements with 2 key
   heads repeated to 4 value heads.
3. When the head counts are equal the module returns the SAME tensor rather than a
   copy. A copy would be correct numerically and wrong structurally - the extra node
   would change the graph the rest of the layer is built from.

## 3. One defect found in the gate, not the module

`ggml_backend_tensor_get` aborted: the test used a host-allocated context
(`no_alloc = false`), so the tensors have data but no backend buffer. Reading
`tensor->data` directly is correct there. The module was never at fault; the
failure was in how the gate read its results.

## 4. State of the recurrent path

| piece | state |
|---|---|
| `ssm_conv` | bit-exact vs `ggml_ssm_conv` (EXP-685g) |
| `gated_delta_net` | bit-exact vs `ggml_gated_delta_net`, 264 192 elements (EXP-685h) |
| `l2_norm` | bit-exact vs `ggml_l2_norm` (this card) |
| `repeat_4d` | bit-exact vs `ggml_repeat_4d`, with the skip arm (this card) |
| the layer wired | not yet |
| conv/ssm state buffers | not yet - they are the recurrent layer's KV cache |

The remaining work on this path is the layer wiring and the state buffers. The
arithmetic is now complete.

## 5. Cost

One arch-unit build (122 modules, 1065 symbols) because the two exports did not exist
in any object, then one gate compile.
