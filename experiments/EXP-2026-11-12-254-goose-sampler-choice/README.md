# EXP254: greedy-vs-distribution sampler decision in Goose (APEX.REQUEST/SESSION)

`common_sampler_init` ends the chain with either a greedy sampler or a seeded distribution sampler. The choice:

    greedy = n_probs == 0 && !samplers.empty() &&
             ((last == TEMPERATURE && temp == 0 && dynatemp_range == 0) ||
              (last == TOP_K && top_k == 1))

A wrong port here silently changes sampling while every structural check still passes.

## Gate

    FG254S_GATE cases=108 failures=0

`n_probs` x `n_samplers` x last-sampler type x `top_k`, against a transcription of the native expression. (The first
version of the gate itself was wrong -- it compared `top_k` in the temperature case -- which is why the gate is
written from the native expression rather than from my module.)

## Live and defect-tested

Wired into `common/sampling.cpp`; the accepted profile completes with `e68df05b`, equal probabilities, restore ok.

Forcing the decision to always-greedy changed the completion to
`9fef37f8a964f1c7d8970e718e983e19d960e246b95928129c4420cd12ea95b3`; restoring returned `e68df05b`.
