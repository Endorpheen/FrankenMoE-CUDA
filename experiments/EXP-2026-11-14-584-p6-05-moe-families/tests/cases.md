# MoE families structure cases (P6-05 architectures 4, 7, 13)

Oracle: the native loader declarations in `src/models/qwen3moe.cpp`,
`src/models/qwen2moe.cpp` and `src/models/glm4-moe.cpp`. The values are read
from those lines; no native library is linked.

The three fixtures share one 21-element parameter layout (the `P_*` constants
in every source file and `tests/cases.md` here):

```
0  n_embd            7  n_embd_head_k    14 n_expert_shared
1  n_vocab           8  n_embd_head_v    15 n_layer_dense_lead
2  n_layer           9  n_ff             16 has_output
3  n_layer_all      10  n_expert         17 qk_norm present
4  n_layer_nextn    11  n_expert_used    18 expert gating func
5  n_head           12  n_ff_exp decl.   19 mrope sections present
6  n_head_kv        13  n_ff_shexp decl. 20 n_rot
```

Every fixture is a separate binary, so the three generated C files never share
a symbol table.

## qwen3moe — FG-ARCH4

Parameters: `n_embd 2048, n_vocab 151936, n_layer 48, n_head 32,
n_head_kv 4, head 128, n_ff 6144, n_expert 128, used 8, n_ff_exp declared 0`.

Derived: `n_ff_exp = 6144/8 = 768`, `n_embd_k_gqa = n_embd_v_gqa = 512`,
`q_width = 128*32 = 4096`, fused QKV `= 4096+512+512 = 5120`.

| # | case | expected | native anchor |
|---|---|---|---|
| 1 | plan | 48 layers, all kind 0 | `qwen3moe.cpp:27-55` |
| 2 | `token_embd.weight` | `{2048, 151936, 1}` | `qwen3moe.cpp:17` |
| 3 | `output_norm.weight` | `{2048, 1, 1}` | `qwen3moe.cpp:20` |
| 4 | fused `attn_qkv.weight` | `{2048, 5120, 1}` | `qwen3moe.cpp:32` |
| 5 | `attn_wo.weight` | `{4096, 2048, 1}` | `qwen3moe.cpp:33` |
| 6 | `attn_q_norm.weight` | `{128, 1, 1}` | `qwen3moe.cpp:36` |
| 7 | `ffn_gate_inp.weight` | `{2048, 128, 1}` | `qwen3moe.cpp:40` |
| 8 | `ffn_gate_exps.weight` | `{2048, 768, 128}` | `qwen3moe.cpp:52` |
| 9 | `ffn_down_exps.weight` | `{768, 2048, 128}` | `qwen3moe.cpp:53` |
| 10 | `ffn_up_exps.weight` | `{2048, 768, 128}` | `qwen3moe.cpp:54` |

Case 1 also checks the output policy (`output_optional = 1`, `:21-25`) and the
absence of shared experts and `attn_post_norm` (`q3m_shape` returns 0).
Expected: `FG-ARCH4 cases=10 values=78 failures=0` (48 plan kinds + 3 policy
values + 9 shapes x 3).

## qwen2moe — FG-ARCH7

Parameters: `n_embd 3584, n_vocab 151936, n_layer 28, n_head 28, n_head_kv 4,
head 128, n_ff 18944, n_expert 64, used 8, n_ff_exp declared 0,
n_ff_shexp 2048`.

Derived: `n_ff_exp = 18944/8 = 2368`, `n_embd_k_gqa = 512`, `q_width = n_embd`,
fused QKV `= 3584+512+512 = 4608`.

| # | case | expected | native anchor |
|---|---|---|---|
| 1 | plan | 28 layers, all kind 0 | `qwen2moe.cpp:25-58` |
| 2 | `token_embd.weight` | `{3584, 151936, 1}` | `qwen2moe.cpp:19` |
| 3 | `output_norm.weight` | `{3584, 1, 1}` | `qwen2moe.cpp:22` |
| 4 | fused `attn_qkv.weight` | `{3584, 4608, 1}` | `qwen2moe.cpp:30` |
| 5 | `attn_k.weight` | `{3584, 512, 1}` | `qwen2moe.cpp:30` |
| 6 | `attn_wo.weight` | `{3584, 3584, 1}` | `qwen2moe.cpp:31` |
| 7 | `ffn_gate_inp.weight` | `{3584, 64, 1}` | `qwen2moe.cpp:35` |
| 8 | `ffn_gate_exps.weight` | `{3584, 2368, 64}` | `qwen2moe.cpp:47` |
| 9 | `ffn_down_exps.weight` | `{2368, 3584, 64}` | `qwen2moe.cpp:48` |
| 10 | `ffn_down_shexp.weight` | `{2048, 3584, 1}` | `qwen2moe.cpp:56` |

Case 1 also checks the mandatory output (`output_optional = 0`, `:23`) and the
absence of q/k norm and `attn_post_norm`.
Expected: `FG-ARCH7 cases=10 values=58 failures=0` (28 plan kinds + 3 policy
values + 9 shapes x 3).

## glm4-moe — FG-ARCH13

Parameters: `n_embd 4096, n_vocab 151552, n_layer 46, n_layer_all 47,
n_layer_nextn 1, n_head 32, n_head_kv 2, head 128, n_ff 11264, n_expert 128,
used 8, n_ff_exp declared 0, n_expert_shared 2, n_layer_dense_lead 1,
qk_norm 1, mrope 1`.

Derived: `n_ff_exp = 11264/8 = 1408`, `n_ff_shexp = 1408*2 = 2816`,
`n_embd_k_gqa = n_embd_v_gqa = 256`, `q_width = 4096`, fused QKV
`= 4096+256+256 = 4608`.

| # | case | expected | native anchor |
|---|---|---|---|
| 1 | plan | layer 0 dense, 1..45 MoE, 46 MTP | `glm4-moe.cpp:49-51`, `:70` |
| 2 | `token_embd.weight` | `{4096, 151552, 1}` | `glm4-moe.cpp:39` |
| 3 | MoE fused `attn_qkv.weight` | `{4096, 4608, 1}` | `glm4-moe.cpp:56` |
| 4 | `attn_post_norm.weight` | `{4096, 1, 1}` | `glm4-moe.cpp:66`, `:369` |
| 5 | `ffn_exp_probs_b.bias` | `{128, 1, 1}` | `glm4-moe.cpp:76` |
| 6 | `ffn_gate_inp.weight` | `{4096, 128, 1}` | `glm4-moe.cpp:74-75` |
| 7 | `ffn_gate_exps.weight` | `{4096, 1408, 128}` | `glm4-moe.cpp:81-82` |
| 8 | `ffn_gate_shexp.weight` | `{4096, 2816, 1}` | `glm4-moe.cpp:90-92` |
| 9 | dense `ffn_down.weight` | `{11264, 4096, 1}` | `glm4-moe.cpp:101` |
| 10 | MTP `nextn.eh_proj.weight` | `{8192, 4096, 1}` | `glm4-moe.cpp:107` |

Case 1 also checks `g4m_kind(47) = -1` (out of range), the mrope decision
(`:6`, `:298`), the optional q/k norm (`:60-64`, absent under `qk_norm = 0`) and
the absence of `ffn_norm` (`:369` uses `attn_post_norm` instead).
Expected: `FG-ARCH13 cases=10 values=78 failures=0` (47 plan kinds + 4 policy
values + 9 shapes x 3).

## Recorded limits

- The gate is structural: it checks plan and shapes against a transcription of
  the three native builders. It does not link the native library and does not
  execute a graph.
- `qwen3moe` has no per-layer kind switch, so its plan case is a constant-kind
  check; `glm4-moe` exercises all three kinds.
- Not covered by cases: qwen3moe `attn_k/v`, qwen2moe `ffn_up_exps` and the
  `ffn_gate_inp_shexp` vector, glm4-moe `ffn_up/down_exps` and the optional
  nextn `embed_tokens`/`shared_head_*` tensors.
- The shared-expert activation path (qwen2moe's sigmoid gate divisor, glm4's
  plain sum) is graph construction and is not part of this slice.
