# EXP-2026-11-15-697 — the MoE router decision, in Goose

Base: `0f0cca1d`. **The module typechecks; it has not been executed.** This card says
so plainly rather than implying a passing gate.

## 1. What was added

`goose/cpu/goose-moe-router.goose`:

| export | decision |
|---|---|
| `moe_softmax` | softmax over one token's logits, max-subtracted, f64 accumulate |
| `moe_top_k` | the `n_used` largest, highest first, lower id wins a tie |
| `moe_weights` | gather by id, optional normalise, then scale (scale arrives as f32 bits) |
| `moe_route` | the three in order, which is `build_moe_ffn`'s selection half |

The native sequence it mirrors is `llama-graph.cpp:1941-2100`:

```
logits  = gate_inp * cur                      [n_expert, n_tokens]
probs   = softmax(logits)
ids     = argsort_top_k(probs, n_expert_used)
weights = get_rows(reshape(probs, [1, n_expert, n_tokens]), ids)
weights = weights / sum_rows(weights)         if norm_w
weights = weights * w_scale
```

`build_moe_ffn` is shared by every MoE architecture, so this module is common. No
architecture gets a private router - rule 17 applied to the FFN.

## 2. Why it is common and why the kernels are not here

The expert arithmetic is already migrated: `goose/cpu/goose-quantized-moe/` holds the
dequantisation, matrix and activation kernels, and the P1-12 SIMD bridge drives them
(16 384 checks, 8192 calls, zero differences). Routing is a different question - which
experts, with what weights - so it is a decision and belongs in Goose, while the kernels
stay where they are.

## 3. Why there is no passing gate yet

A self-check harness was written and hit language-level problems, not model-level ones:

1. `-` is not allowed in an import path, so `goose-moe-router.goose` cannot be imported
   by name. Modules with a dash enter a program only through composition, which is how
   the arch unit works.
2. After composing the module with the harness by concatenation, the checker refused
   `var logits: f32[N]` followed by per-index assignment: "variable logits may be used
   before it is assigned". The harness needs a definite-assignment form the checker
   accepts before it can run.

The harness was removed rather than left failing in the tree.

## 4. On the accumulate width

`moe_softmax` and `moe_weights` accumulate in f64 while ggml's softmax accumulates in
f32. That is a deliberate choice for numerical headroom, and it means the weights will
NOT be bit-identical to `ggml_soft_max` - they will agree to within f32 rounding. The
expert IDS must match exactly, since they are a comparison of floats that were already
rounded into the same f32 probabilities.

This distinction has to be stated before the gate is written, because a bit-exact
expectation here would be wrong and would send the next attempt chasing a difference
that is supposed to exist.

## 5. Next step

Write the harness in a form the checker accepts, then compare ids exactly and weights to
f32 rounding against a ggml chain of `softmax` + `argsort_top_k` + `get_rows` +
`sum_rows` + `scale`. Only then the expert matmuls and the layer stack.
