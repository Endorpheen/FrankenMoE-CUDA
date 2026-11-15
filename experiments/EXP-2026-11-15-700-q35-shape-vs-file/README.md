# EXP-2026-11-15-700 — every declared tensor shape matches the file

Base: `9cda831f`. No model run, no GPU.

## 1. Result

```
FG_Q35_SHAPE checked=30 mismatch=0 absent=0 PASS
```

Thirty tensors across one recurrent layer (0) and one full-attention layer (3): the shape
`q35_shape` declares for each equals the extent in the GGUF, exactly, including the
branch-specific ones.

## 2. What is covered

| layer kind | tensors |
|---|---|
| both | `attn_norm`, `post_attention_norm`, `ffn_gate_inp`, the four expert matrices, the four shared-expert matrices |
| full attention (3) | `attn_output`, `attn_q_norm`, `attn_k_norm` |
| recurrent (0) | `ssm_conv1d`, `ssm_dt`, `ssm_a`, `ssm_beta`, `ssm_alpha`, `ssm_norm`, `ssm_out` |

The branch filtering is what makes this more than a shape dump: a recurrent layer is never
asked about `attn_output` and an attention layer is never asked about `ssm_a`, so the
module's own decision about which tensors a kind carries is exercised, not bypassed.

## 3. What was deliberately left out

The `attn_qkv` / `attn_q` / `attn_k` / `attn_v` / `attn_gate` family. Their names are
unambiguous in `qwen35moe.cpp:77` and `:87-88`, but the module carries two ids that both
refer to a qkv projection (`T_ATTN_QKV` and `T_WQKV`) and their intended split is not
stated in the code. Mapping them by guess would produce differences that are the gate's
fault - and this project has already spent time on exactly that class of error. They are
left for a pass that starts by reading what the module intends, not by inferring it from
the id ordering.

## 4. Why this matters for the stack

The stack cannot be built until every tensor's shape is known and correct. This check
establishes that for 30 of them against the file rather than against another copy of the
same arithmetic - the distinction that mattered in EXP-699, where the plan agreed with
itself and disagreed with the weights.

## 5. Cost

Gate compiles only.
