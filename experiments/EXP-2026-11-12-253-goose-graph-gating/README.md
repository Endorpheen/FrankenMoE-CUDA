# EXP253: expert-gating decision of build_moe_ffn in Goose (APEX.MODEL_GRAPH, first node)

`build_moe_ffn` selects the gating function that turns router logits into expert probabilities. The tensor
operations stay ggml primitives -- they are the graph layer, like the allocator and the backend calls. What Goose
owns is which primitive applies, and the abort on an unknown type.

## The enum matters

`llama_expert_gating_func_type` is `NONE=0, SOFTMAX=1, SIGMOID=2, SOFTMAX_WEIGHT=3, SQRT_SOFTPLUS=4`. The first
version of the module assumed 0..3 and produced `e32fb895...` instead of `e68df05b`. That is exactly the kind of
silent mistake this item can hide, and the accepted-profile check caught it immediately.

## After the fix

    first_sha = e68df05b4829865dcf8cd2424febfaded15b055f56fec36fefc002650ef8e7c9
    probs_equal = true, restore_ok = true

## Defect test (A -> B -> A)

Making `SOFTMAX` behave as `SIGMOID` inside the Goose module and rebuilding only the server changed the completion
back to `e32fb895...`; restoring returned `e68df05b`. So this decision is on the live graph-construction path.

## What is ported and what is not

Ported: the gating selection (five cases including the abort). Not ported: the rest of `build_moe_ffn` -- expert
selection bias, top-k routing, the expert matmuls and the weighted aggregation. Those need the wider tensor
surface in the bridge, which is the next concrete step for this item.
