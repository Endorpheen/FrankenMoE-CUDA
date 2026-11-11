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

## Attempt: normalising the expert weights (reverted)

Tried to port the `norm_w` block as well: reshape to 2D, `sum_rows`, clamp the sum to `6.103515625e-5 .. +inf`,
divide. Two things were caught along the way and both are worth recording:

- the first constant was `0x3a800000`, which is `2^-10`, not the F16-smallest `2^-14 = 0x38800000`; the correct
  value was verified with a bit-level check before changing the code;
- with the guard fixed and the constant corrected, the completion was still `e611eee2...` instead of `e68df05b`, so
  the ported sequence is not equivalent to the native one. The likely difference is the `n_tokens == 0` graph-build
  case, where the native code still reshapes and mine returns the input unchanged.

Reverted to the native block; the accepted profile is back to `e68df05b`. The gating decision stays ported and
defect-tested. A correct `norm_w` port needs the graph-build case handled deliberately, not bypassed.

## norm_w: corrected and now live

The earlier failure had one cause, and it was mine: I special-cased `n_tokens == 0` by returning the input unchanged,
while the native code reshapes unconditionally, and I added a redundant `reshape_2d` around the row sum. Removing the
special case and the extra reshape made it exact:

    first_sha = e68df05b...
    probs_equal = true, restore_ok = true

Defect test (A -> B -> A): raising the clamp lower bound from the F16-smallest to 1.0f changed the completion to
`20f0af5d...`; restoring returned `e68df05b`. So the normalisation sequence is on the live graph path.

## top-k expert selection: third graph decision, live

`selected_experts = ggml_argsort_top_k(ctx0, selection_probs, n_expert_used)` is now decided in Goose (mode 3),
with ggml doing the sort through the bridge.

    first_sha = e68df05b..., probs_equal, restore_ok

Defect test: selecting `n_used - 1` experts instead of `n_used` made the server crash while building the graph --
the downstream gather no longer matches. A crash in a graph whose shape is derived from this value is proof the
decision is on the live path; the accepted profile is restored to `e68df05b` afterwards.

Three graph decisions are now Goose and tested: gating selection, expert-weight normalisation, and top-k selection.
