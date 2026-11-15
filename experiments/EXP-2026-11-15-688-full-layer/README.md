# EXP-2026-11-15-688 — one Apex attention layer, wired end to end

Base: `161f43c8`. No model run, no GPU. Reads the model file for extents only.

## 1. The result

```
FG_APEX_LAYER nodes=15 K=2048 H=16 HKV=2 HD=256 HDKV=256 out=[2048 1]
              shape_ok=1 status=0,0 nonfinite=0 changed=2048 nondet=0 PASS
```

Fifteen nodes, built by `gb_common` plus the QG split module, executed on the CPU
backend as one graph at the model's own geometry:

```
x -> rms_norm -> wq -> QG split -> q, gate
              -> wk -> view -> k   -> rope
              -> wv -> view -> v
rope(q) x rope(k) x v -> flash_attn -> * sigmoid(gate) -> wo -> + x
```

`changed=2048` means every output element differs from the residual input, so the
graph is not a pass-through. `nondet=0` means two runs of the same graph are
bit-identical. `nonfinite=0` means no NaN or inf anywhere.

## 2. Four things the model corrected, each caught by an assertion

Every one of these would have produced a graph that runs and computes the wrong
thing:

1. **`n_embd_head` is 256, not 512.** The q projection emits two head widths per
   head (query and gate interleaved), so it is `wq_n/(2*n_head)`. Using `wq_n/n_head`
   made the split test agree with itself while describing a layout the model does
   not have.
2. **The gate is `sigmoid`, not `silu`.** `qwen35moe.cpp:352` uses `ggml_sigmoid`.
3. **The gate needs a reshape before the multiply.** The attention result is
   `[HD, 1, H, 1]` while the gate view is `[HD, H, 1, 1]`; ggml's `mul` asserts the
   shapes match, and the gate is reshaped to the attention layout (same 4096
   elements).
4. **mode 26's argument order.** `arg7` is the mask pointer, `arg8` the scale. Passing
   the scale in `arg7` made the module treat a float bit pattern as a tensor pointer
   and crashed in `fg158_prop` — a loud failure, which is the point of the binding
   reading handles rather than guessing.

## 2b. The wiring is proven against ggml's own constructors

The gate now builds the same layer twice: once through `gb_common` and the QG split
module, once with `ggml_rms_norm`, `ggml_mul_mat`, `ggml_view_3d`, `ggml_rope_ext`,
`ggml_flash_attn_ext`, `ggml_sigmoid`, `ggml_mul`, `ggml_add`. Eleven intermediate
tensors and the final output are compared bit for bit:

```
cmp normed   n=2048 differs=0     cmp attn   n=4096 differs=0
cmp qg       n=8192 differs=0     cmp g4     n=4096 differs=0
cmp k4       n=512  differs=0     cmp gs     n=4096 differs=0
cmp v4       n=512  differs=0     cmp gated  n=4096 differs=0
cmp qr       n=4096 differs=0     cmp proj   n=2048 differs=0
cmp kr       n=512  differs=0
wiring_differs=0 vs_ggml=0
```

This is the strongest statement available without a reference model run: the
Goose-built graph is the same graph a native builder would produce, node for node
and element for element.

It also caught the last real defect: the first comparison diverged in every rope
element because the layer passed only `freq_base`. `ggml_rope_impl` also needs
`freq_scale`, `ext_factor`, `attn_factor`, `beta_fast` and `beta_slow`, and the
per-node rope gate had passed precisely because it supplied all six. Comparing
intermediates located it in one run.

## 3. What the gate does and does not prove

**Proves**: the fifteen nodes are wired in the right order with the right shapes, the
graph executes, the output is finite, it is not the input, and it is deterministic.
Each node's arithmetic is proven bit-exact against the substrate in EXP-681/685.

**Does not prove**: that a 42-layer stack, KV cache and sampler produce the reference
token sequence. That needs the stack and a model run. What it does prove is that one
layer's graph is identical to the graph a native builder would emit - so a remaining
mismatch could only come from the layers above it, not from this one.

## 4. Where this leaves the runtime

| layer | state |
|---|---|
| one attention layer, 15 nodes | **works**, deterministic |
| one recurrent layer | the GDN nodes are proven individually; not yet wired |
| 42-layer stack | not built |
| KV buffers, prefill, decode | not built |
| CUDA | not built |
| tokenizer, HTTP | not built |

## 4b. The recurrent layer: what it needs, read from the native builder

31 of the 42 layers are recurrent, so this is the majority path. `qwen35moe.cpp`
:404-470 gives the order, and it needs more than the attention layer did:

```
conv_states = get_r_l(il)          persistent per-layer conv state
ssm_states  = get_s_l(il)          persistent per-layer SSM state
conv_input  = build_conv_state(inp, conv_states, qkv_mixed, kernel_size, channels)
state       = reshape(get_rs(...), head_v_dim, head_v_dim, num_v_heads, n_seqs)
conv_out    = ggml_ssm_conv(conv_input, ssm_conv1d)
conv_silu   = ggml_silu(conv_out)
q/k/v       = view_4d over conv_silu with the qkv stride
q, k        = ggml_l2_norm(q, eps), ggml_l2_norm(k, eps)
repeat      = q/k repeated to num_v_heads when the head counts differ
...         = gated_delta_net(q, k, v, gate, beta, state)
```

Three things the runtime does not have yet, and each is a specific piece rather
than a vague gap:

1. **State buffers.** `conv_states` and `ssm_states` are the recurrent layer's KV
   cache: they persist across tokens and are read and written by every step. The
   one-layer gate can pass a zeroed state, but prefill and decode cannot.
2. **`l2_norm`.** The module has 28 modes and none of them is `ggml_l2_norm`, so a
   new mode or export is needed. `ggml_repeat_4d` is likewise absent.
3. **The qkv stride view.** The conv output carries q, k and v at a
   `head_k_dim*num_k_heads*2 + head_v_dim*num_v_heads` stride, which is a
   `view_4d` with an offset - the primitive added in EXP-687c covers this.

Node-level work for that path is already proven: `ssm_conv` and
`gated_delta_net` are bit-exact against the substrate (EXP-685g/685h). What is
missing is the state plumbing and two ops.

## 5. Cost

One arch-unit build (for `qg_split`), then gate compiles only. No model was started.

## 6. Reproduce

```
./builds/goose-standalone/apex_layer <apex-model.gguf>
```
