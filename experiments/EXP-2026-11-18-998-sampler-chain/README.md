# EXP-2026-11-18-998: The sampler chain, measured against the samplers

Status: `sampler-chain-full.goose` joined the unit and is proven equal to the real
libllama chain on nine chain configurations and four thousand generator draws. 145
modules, 38 302 checks.

## Why this one

The unwired list from EXP997 named it as the most valuable entry: the module holds the
whole chain including the draw, the unit carried `sampler-chain` and the three filters
but not the draw, so **sampling still ran in libllama**. That is the P6-10 remainder, and
a chain that is only "similar" produces a different sentence rather than an error.

## What was done

The module joined the composed unit (144 -> 145) with no conflicts: its exports are
`sm_*`, its private helpers are already prefixed, and its two externs are `expf` and
`logf` - libm, which the project requires instead of language builtins.

A differential gate was written against things that are not my own code:

| differential | oracle | result |
| --- | --- | --- |
| `sm_mt_next` | `std::mt19937` | **bit-identical**, 4 seeds x 1000 draws |
| `sm_select` with top_k 20 | libllama chain `top_k` | token and index match, 3 seeds |
| `sm_select` with top_k + top_p 0.95 | libllama chain `top_k -> top_p` | match, 3 seeds |
| `sm_select` with top_k + top_p + min_p 0.05 | libllama chain `+ min_p` | match, 3 seeds |

The draw had to be right: native uses `std::uniform_real_distribution<double>`
(`llama-sampler.cpp:1213`), which is `generate_canonical<double, 53>` - two 32-bit draws
divided by 2^64. `mt_canonical` does exactly that. Had it used one draw over 2^32 (the
`float` form), every token after the first would have differed.

## A gate bug, caught by disagreeing with itself

The first run failed all three chain cases with goose returning 0, 1, 2. That is not a
near-miss, it is a pattern - and the pattern was mine: `sm_select` writes **out[0] as the
index** in the candidate array and **out[1] as the token id**, and the gate compared
out[0] with the native token id. The module was right; the check was reading the wrong
word.

Corrected to compare out[1]; 3/3 per case. Worth recording because the failure looked
like a real arithmetic mismatch and would have been reported as one.

## Result

| gate | checks | result |
| --- | --- | --- |
| sampler-chain | 7 | PASS |
| whole suite | **38 302** | **0 failures** (11 gates) |
| unit | **145 modules**, typecheck clean, no conflicts |
| unwired list | 89 -> **88**, baseline refreshed |

## Not claimed

**The chain is in the unit but not yet on the shipping path.** `llama-sampler.cpp` still
builds the native chain; the module is proven equal, not wired. Wiring means replacing
the sampler objects in the shipped library, which changes generation and needs the full
suite plus a decision about `llama_sampler_i` - the next step, not this one.

`sm_select` returns `-2`/`-3` when top_k is disabled and top_p/min_p are on, because the
unsorted branch is not ported. The naive chain always has top_k enabled, so the gate
covers the path that runs; the other is a stated limit, not a silent one.

The gate exercises one candidate width (32) and three seeds per case. That is a
differential, not a proof over all inputs.

No model was started, no benchmark was run.
