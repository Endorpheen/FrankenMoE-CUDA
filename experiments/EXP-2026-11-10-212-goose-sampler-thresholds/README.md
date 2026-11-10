# EXP212: sampler thresholds in Goose — corrections to my own verdict

`top_k_cut`, `min_p_scan` and `top_p_scan` from llama-sampler.cpp (the arithmetic behind
`--top-k 20 --top-p 0.95 --min-p 0`). Correct, live, and **3.4-3.6% slower on decode** — rejected.

## Verdict — corrected

**NOT PROMOTED, but my first rejection reason was wrong.** I rejected on a −3.35% / −2.90% decode
delta measured by comparing *non-interleaved* runs. That is exactly the incomparable-build mistake
the project rules forbid. An interleaved A/B pair sequence shows a different number.

### Interleaved measurement (A = EXP211, B = EXP212, alternating)

| pair | A decode | B decode | delta |
|---|---:|---:|---:|
| 1 | 24.98 | 24.49 | −1.96% |
| 2 | 25.97 | 25.54 | −1.66% |
| **mean** | **25.48** | **25.02** | **−1.81%** |

−1.81% is **inside** the 2% rule. My earlier −3.35% came from a baseline that happened to run high
(26.29, 26.24) against a candidate that ran low.

### Why it still is not promoted

The absolute decode floor is the problem, not the 2% rule. Both arms now sit at or below 25:
the baseline itself measured 24.44, 24.98 and 25.97 in this period, i.e. it fell below the floor
on its own. The environment moved between the earlier runs and these, and I cannot attribute a
−1.8% difference to code when the baseline alone swings 1.5 tok/s (−6%).

So: the sampler port is **within** 2% of baseline under pairing, but neither the candidate nor the
baseline currently clears the absolute 25 tok/s floor reliably. Promoting on this evidence would
mean promoting into an unstable environment. It stays out of the chain until a stable window
exists; the code and gate are kept.

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

## Mechanism, not established as the cause

Per decode step the sampler works on a tiny array (~20 candidates after top-k), so the arithmetic
is negligible and any cost is the marshal: logits into `std::vector<float>`, a `std::vector<int64_t>`
keep array, then the ABI call, per token and several samplers deep.

That is a plausible mechanism of the same O(copy) shape as EXP204, but **I have not proven it is
the cause of the −1.8%** — under interleaving the effect is near the measurement noise, so the
marshal hypothesis stays a hypothesis. Recording it as such.

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
