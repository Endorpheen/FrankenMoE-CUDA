# EXP212: sampler thresholds in Goose — REJECTED on decode regression

`top_k_cut`, `min_p_scan` and `top_p_scan` from llama-sampler.cpp (the arithmetic behind
`--top-k 20 --top-p 0.95 --min-p 0`). Correct, live, and **3.4-3.6% slower on decode** — rejected.

## Verdict

**REJECTED.** No promotion, switch stays opt-out default OFF is not applicable here: the code is
wired but disabled by leaving it out of the chain.

    gate:                     FG212S_FIXTURE checks=5654 failures=0
    live witness:             FG212S_SAMPLER min_p=0 top_p=136 top_k=136
    tokens and probabilities: exact (sha e68df05b)

| run | prefill | decode |
|---|---:|---:|
| EXP211 (baseline, re-measured) | 2544.27 | **26.24** |
| EXP212 first | 2539.75 | **25.36** |
| EXP212 repeat | 2560.02 | **25.48** |

Decode delta −3.35% / −2.90%, both beyond the 2% rule. Prefill is within noise (+0.6% / +0.6%),
so this is not general noise: two independent runs of the new build land below two independent
runs of the baseline.

## Why it is slower

Per decode step the sampler runs on a tiny array (the candidate set, ~20 after top-k), so the work
is negligible and the cost is the **marshal**: copying logits into a `std::vector<float>`, then a
`std::vector<int64_t>` keep-flag array, then calling across the ABI — all per token, several
samplers deep. Native reads the array in place.

That is the same O(copy) shape as the EXP204 mistake, just much smaller: the array is 20 entries
rather than 262144, so it costs a few percent instead of 59%. But the per-token frequency makes
even a few percent visible.

## What would make it viable

The module is correct and stays as the verified implementation. To make it pay, the arrays must
stop being copied: either the sampler side exposes its `llama_token_data` storage directly (which
needs the struct layout in Goose, i.e. typed slices — the same blocker as MMQ), or the thresholds
must be decided once per batch rather than per token where the engine allows it.

## What is verified

`min_p_scan` and `top_p_scan` are compared against transcriptions of the native loops over 400
randomised cases each, including `min_keep` interaction and the `p <= 0` / `p >= 1` early exits.
`top_k_cut` covers `k <= 0` and `k > size`. The `min_p=0` counter in the live witness is correct:
the profile passes `--min-p 0`, which takes the native early return.

## Files kept

`source/samp.goose` and the gate are the verified implementation. `integration/llama-sampler.cpp`
holds the wiring; it is **not** in the active chain, so the accepted runtime stays EXP211.
