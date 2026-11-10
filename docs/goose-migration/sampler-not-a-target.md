# The sampler is not a port target, and the reason is the working-set size

Before porting `llama_sampler_softmax_impl` I checked how many elements it actually touches. It is 20,
not 150,000.

## Why

`common.h` builds the default chain in this order:

    PENALTIES, DRY, TOP_N_SIGMA, TOP_K, TYPICAL_P, TOP_P, MIN_P, XTC, ..., then TEMPERATURE / DIST

`llama_sampler_top_k_impl` sorts and then does `cur_p->size = k`. The Apex profile runs `--top-k 20`,
so everything after it — including the softmax inside `temp`/`dist` — sees **20 elements**.

`llama_sampler_softmax_impl` is three loops over `cur_p->size`, so per request it is about
128 tokens × 20 values ≈ 2,560 element operations. That is nothing next to 2 million `unicode_cpt_from_utf8`
calls or 255,553 whole-string decodes in the same request.

## Why this matters beyond the sampler

A loop that looks like a whole-vocabulary scan is not one, because an earlier stage truncated the array.
Porting it would have produced a component that is correct, measurable, and worth zero — and the
measurement would have shown "no effect", which is exactly the kind of null result that costs a stage
and teaches nothing.

The check is cheap: read the chain order and the size at the point of interest, not the function name.

## What this does not say

It does not say the sampler is untouchable forever. `--top-k 0` (no truncation) would make softmax a
full-vocabulary pass. That is a different profile; the Apex profile is not it.

It also does not revisit EXP212's rejection of the sampler *thresholds* (top_k_cut, min_p_scan,
top_p_scan) at −3.5% decode. That verdict stands on its own measurement, on arrays that were still
large; this note is about softmax specifically, whose array is already 20 by then.
