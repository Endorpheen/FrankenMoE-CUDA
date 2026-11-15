# EXP-2026-11-15-700 — every declared tensor shape matches the file

Base: `9cda831f`. No model run, no GPU.

## 1. Result

```
FG_Q35_SHAPE checked=32 mismatch=0 absent=0 PASS
```

Thirty-two tensors across one recurrent layer (0) and one full-attention layer (3): the shape
`q35_shape` declares for each equals the extent in the GGUF, exactly, including the
branch-specific ones.

## 2. What is covered

| layer kind | tensors |
|---|---|
| both | `attn_norm`, `post_attention_norm`, `ffn_gate_inp`, the four expert matrices, the four shared-expert matrices |
| full attention (3) | `attn_output`, `attn_q_norm`, `attn_k_norm` |
| recurrent (0) | `attn_qkv`, `attn_gate`, `ssm_conv1d`, `ssm_dt`, `ssm_a`, `ssm_beta`, `ssm_alpha`, `ssm_norm`, `ssm_out` |

The branch filtering is what makes this more than a shape dump: a recurrent layer is never
asked about `attn_output` and an attention layer is never asked about `ssm_a`, so the
module's own decision about which tensors a kind carries is exercised, not bypassed.

## 3. The two qkv ids, resolved by reading the module

The module carries `T_ATTN_QKV` (5) and `T_WQKV` (9), and the first pass left both out
rather than guess. Reading `qwen35moe.goose:186-193` resolves it:

```goose
if tid == T_ATTN_QKV { q_put3(out, n_embd, qkv_fused, 1); }   // attention branch
if tid == T_WQKV    { q_put3(out, n_embd, qkv_w,     1); }   // recurrent branch
if tid == T_WQKV_GATE { q_put3(out, n_embd, value_d, 1); }
```

with `qkv_fused = q_width + n_embd_k_gqa + n_embd_v_gqa` - a **single fused q+k+v**
tensor - while `qkv_w` is the recurrent branch's `attn_qkv` width.

So:

- `T_WQKV` (9) is `blk.N.attn_qkv.weight` and `T_WQKV_GATE` (10) is
  `blk.N.attn_gate.weight`. Both are real in this file and both are now checked.
- `T_ATTN_QKV` (5) describes a fused q+k+v layout that this GGUF does not use: it carries
  separate `attn_q`, `attn_k` and `attn_v`. There is nothing to compare it against, so it
  is excluded on the grounds that the file has no such tensor - not on the grounds that
  its meaning was unclear.

The distinction matters: the first pass excluded it because the meaning was unclear, this
pass excludes it because the meaning is now known and the tensor is absent.

## 4. Why this matters for the stack

The stack cannot be built until every tensor's shape is known and correct. This check
establishes that for 30 of them against the file rather than against another copy of the
same arithmetic - the distinction that mattered in EXP-699, where the plan agreed with
itself and disagreed with the weights.

## 5. Cost

Gate compiles only.
