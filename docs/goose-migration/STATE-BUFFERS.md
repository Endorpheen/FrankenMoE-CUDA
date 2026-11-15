# State buffers — the recurrent layer's KV cache

Base: `2d8ac744`. Requirements read from `delta-net-base.cpp:449-496` and
`qwen35moe.cpp:404-470`. Nothing here is guessed.

## 1. What the native code does

`build_conv_state` is the whole mechanism, and it is short:

```c
conv_states = build_rs(inp, conv_states_all, n_embd_r(), n_seqs);      // per-layer slot
conv_states = ggml_reshape_3d(ctx0, conv_states, kernel_size - 1, channels, n_seqs);

qkv_mixed   = ggml_transpose(ctx0, qkv_mixed);                          // [C,T] -> [T,C]
conv_input  = ggml_concat(ctx0, conv_states, qkv_mixed, 0);             // [kernel-1+T, C]

conv_output = ggml_ssm_conv(ctx0, conv_input, kernel);

// the new state is the TAIL of conv_input, written back into the layer's slot
conv_state_last = ggml_view_3d(ctx0, conv_input, kernel_size - 1, channels, n_seqs,
        conv_input->nb[1], conv_input->nb[2], ggml_row_size(type, s_idx));
conv_state_update = ggml_view_2d(ctx0, conv_states_all, row_count, n_seqs,
        nb[1], (slot * mem_size + kv_head) * row_size);
ggml_build_forward_expand(gf, ggml_cpy(ctx0, conv_state_last, conv_state_update));
```

So the state is not a separate mechanism: it is a **concat at the front and a `cpy`
of the tail at the back**, on the same graph, every token. `ssm_states` works the
same way through `build_rs`, and `gated_delta_net` writes it back through the same
`cpy` pattern.

## 2. Three things are missing, all of them specific

1. **`transpose`.** The graph builder's 28 modes have no transpose, and the conv
   state update needs `qkv_mixed` transposed from `[channels, tokens]` to
   `[tokens, channels]` before it can be concatenated along dim 0. `concat` (mode 35)
   and `cpy` (mode 47) exist.
2. **The state slot addressing.** `(slot * mem_size + kv_head) * row_size` is a
   view offset into one flat state tensor that holds every layer and every sequence.
   That arithmetic is a decision and belongs in Goose, beside `gdn_qkv_view`.
3. **Persistence between calls.** The graph is currently built with fresh tensors per
   call. A state that survives from token 1000 to token 1001 needs a tensor outside
   the per-call context and a stable slot offset. This is the one piece that is
   runtime structure rather than graph arithmetic, and it is the same shape as the
   attention KV cache - so both caches should be built by one mechanism, not two.

## 3. Order of work, and why this is a card rather than a commit

1. Add the `transpose` primitive and a `gdn_conv_state` export that decides the
   concat, the tail view and the slot offset. One arch-unit build.
2. Build the state allocator once and use it for both the attention KV cache and the
   conv/ssm state. This is structure, so it gets designed before it is written.
3. Gate a two-token sequence against native ggml ops: token 0 with a zeroed state,
   token 1 reading what token 0 wrote. That is the only check that actually exercises
   recurrence - a single-token gate cannot, because with a zeroed state the state
   path is an identity.

Step 3 is the acceptance criterion for the whole piece. Until a second token reads
the first token's state correctly, "recurrent" is unproven no matter how many nodes
are bit-exact.

## 4. What is already proven and does not need repeating

| piece | evidence |
|---|---|
| attention layer, 15 nodes | bit-identical to a native build, EXP-688b |
| recurrent chain, 10 nodes | bit-identical to a native build, EXP-690/691 |
| `ssm_conv`, `gated_delta_net` | bit-exact vs the substrate, EXP-685g/685h |
| `l2_norm`, `repeat_4d` | bit-exact vs ggml, EXP-689 |

The arithmetic is done. What remains is state, stack and transport.
