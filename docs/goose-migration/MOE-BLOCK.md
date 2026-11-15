# The MoE block — requirements, read from the native code

Base: `fff536e8`. This card records what the FFN block of every Apex layer needs, so
the next step starts from requirements rather than from discovery. No code is claimed.

## 1. The sequence, from `qwen35moe.cpp:517-551`

```c
moe_out = build_moe_ffn(cur,
              ffn_gate_inp, ffn_up_exps, ffn_gate_exps, ffn_down_exps,
              nullptr, n_expert, n_expert_used,
              LLM_FFN_SILU, true, expert_weights_scale,
              LLAMA_EXPERT_GATING_FUNC_TYPE_SOFTMAX, il,
              nullptr, ffn_gate_up_exps, ...);

ffn_shexp = build_ffn(cur, ffn_up_shexp, NULL, ffn_gate_shexp, NULL,
                      ffn_down_shexp, NULL, LLM_FFN_SILU, LLM_FFN_PAR, il);

shared_gate = sigmoid(build_lora_mm(ffn_gate_inp_shexp, cur));
ffn_shexp   = ggml_mul(ffn_shexp, shared_gate);
cur         = ggml_add(moe_out, ffn_shexp);
```

Two branches that are added, and the shared one has its own sigmoid gate. Note the
shared gate is a *separate* projection, one value per token - not the router.

## 2. The parameters, from this GGUF

| key | value |
|---|---|
| `expert_count` | 256 |
| `expert_used_count` | 8 |
| `expert_feed_forward_length` | 512 |
| `expert_shared_feed_forward_length` | 512 |

Tensor shapes the router and experts need are already decided by `q35_shape` in
`goose/model/qwen35moe.goose` - `T_FFN_GATE_INP`, `T_FFN_GATE_EXPS`, `T_FFN_UP_EXPS`,
`T_FFN_DOWN_EXPS`, `T_FFN_GATE_INP_SHEXP`, `T_FFN_GATE_SHEXP`, `T_FFN_UP_SHEXP`,
`T_FFN_DOWN_SHEXP`. That part is done and does not need revisiting.

## 3. What already exists and what does not

**Exists:** the CPU quantised MoE arithmetic. `goose/cpu/goose-quantized-moe/` holds
the dequantisation, matrix and activation kernels, and the P1-12 SIMD bridge drives
them (16 384 checks, zero differences; 8192 calls, zero differences). The arithmetic
of the experts is not the gap.

**Missing:** the graph-level decisions.

1. The router: `logits = gate_inp * cur`, softmax over 256, top-k of 8, and the
   `expert_weights_scale` factor. `build_moe_ffn` lives in llama-graph.cpp and is
   shared by every MoE architecture, so this is a common module, not an Apex one -
   it belongs beside the graph builders, and every MoE arch gets it.
2. The expert matmuls through `mul_mat_id` with the selected ids, and the weighted
   sum back to `n_embd`.
3. The shared expert's own sigmoid gate and the final add.

## 4. Order of work

1. The router decision in Goose: softmax over `n_expert`, top-k `n_expert_used`, the
   scale, and the norm/weight application - as a common module, since `build_moe_ffn`
   is shared.
2. The graph gate: build the MoE block twice, once through the Goose module and once
   with ggml's constructors, and compare the router logits, the selected ids, the
   weighted sum and the final `ffn_out`.
3. Only then the 42-layer stack, which needs attention + recurrent + MoE together.

## 5. The rule this must follow

From EXP-694 and EXP-696: restate the native sequence line by line first, use one
expression per node per arm, and drive the bindings through a named-parameter helper
rather than hand-assembled arrays. Three separate mistakes in the last two attempts
came from argument slots, not from the model.

## 6. State

| piece | state |
|---|---|
| attention layer, 15 nodes | bit-identical |
| recurrent layer, 13 nodes | bit-identical |
| layer kind plan, 42 layers | verified |
| MoE block | requirements recorded; arithmetic exists; graph wiring not started |
| state between calls, stack, prefill, decode, CUDA, tokenizer, HTTP | not started |
