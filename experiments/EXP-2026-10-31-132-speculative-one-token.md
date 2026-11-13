# EXP-2026-10-31-132: native one-token speculation

Status: REJECTED for baseline promotion (diagnostic screen).
Reference: accepted EXP107, commit d73f7d59bd0b50a4f0310759be25af61961cbfd1.

Hypothesis: one speculative token amortizes CPU expert reads enough to offset
proposal, verification and recurrent-state handling costs. Keep all accepted
optimizations and resource placement. No source change or new weights.

Six native candidate configurations and two controls,16 manually issued serial
requests total: fixed32K/128 and repeated-code839/256, greedy. MTP1, simple12/1,
map-k12/1, map-k4v12/1, mod24/min1/max1; simple1/1 replaces inactive simple12/1.
The replacement follows the source-discovered size_m >= size_n requirement.

Clean code control28.8954tok/s; MTP19.3578, map-k27.5772, map-k4v27.0720,
mod27.9262, valid simple20.6157. Inactive simple28.7436 emits no drafts and is
not a speculative performance result. No candidate qualifies for five pairs.
MTP accepts127/127 on code but is33.01% slower in this diagnostic observation.

All selected IDs match. MTP/simple1 code fail ordered-top5/probability tolerance
(maximum delta0.0038994 >0.001); the other profiles are exact. No quality claim.
MTP sampled code peak/retained3762/3342MiB versus3348/2928MiB. Ngram unchanged.

Six initial long windows contain system swap-out and cannot support clean
full-request speed claims. Initial code control has2 major faults; end control
and all candidate code windows have full coverage and zero resource violations.

Ngram-cache fixed8-token implementation and unselected external drafter weights
are outside the one-token screen. No universal rejection of speculative decoding.
EXP107 restored healthy/idle on8081 with all nine mapped hashes matching.

[Evidence, limitations and reproduction](../results/exp132-spec-one-token/README.md).
