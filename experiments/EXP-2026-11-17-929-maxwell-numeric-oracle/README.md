# EXP-2026-11-17-929 — numeric oracle for the Maxwell trunk

Status: ACCEPTED (F32 semantics; synthetic weights)

## Hypothesis

The runtime graph implements the HF Maxwell trunk, not merely something that
runs: with F32 weights and F32 KV, an independent transcription of
`modeling_maxwell_moe.py` must reproduce the runtime logits.

## What is compared

`tests/numeric.py` reimplements, in numpy and independently of the Goose/ggml
code: RMSNorm (weight after the norm), RoPE `rotate_half`, QK-norm, GQA
repetition, causal softmax attention, sigmoid router with expert bias and top-k
renormalisation, routed and shared experts, dense FFN, mHC `pre`/aggregate,
Sinkhorn (softmax + eps, one column norm, then 19 x row/column), `post`
(expand + mix), the learned HC head, final norm and LM head.

The model is the synthetic 2-layer Maxwell from EXP-927 (hidden 64, 8 experts,
1 dense + 1 MoE layer), converted with `--f32 --stub-vocab` so quantisation is
out of the comparison.

## Result

- First run: `max_abs=0.146` — mismatch.
- Component probes (`tests/probe.py`) zeroed one part at a time:
  - attention zeroed: `max_abs=5.5e-8` (HC, MoE, FFN and HC head are exact);
  - FFN zeroed: `max_abs=0.128`, so the residual error was in attention.
- Cause: the model was registered with `LLAMA_ROPE_TYPE_NORM`, which rotates
  adjacent pairs `(i, i+1)`; HF `rotate_half` rotates `(i, i + head_dim/2)`,
  which ggml calls `GGML_ROPE_TYPE_NEOX`. Maxwell's Q/K weights are not
  permuted at conversion, so NEOX is the correct type.
- After the fix: `max_abs=1.73769e-4`, `mean_abs=4.24e-5`, `argmax` matches
  (204), `MAXWELL-NUMERIC OK`.

## What this proves and what it does not

Proves: the trunk decisions and the graph wiring (views, offsets, order of
operations, Sinkhorn layout, GQA, causal mask) match the reference in F32.
Does not prove: quantised (Q4_0/F16) accuracy, real weights, text quality,
speed, memory, or the block-sparse path above 4096 tokens.

## Gate

```
python3 experiments/EXP-2026-11-17-929-maxwell-numeric-oracle/tests/numeric.py
```
