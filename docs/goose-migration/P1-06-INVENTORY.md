# P1-06 inventory: raw quantized MoE math families

Status: inventory step, item stays IN_PROGRESS. Basis: `ggml.h` type list against Goose sources that contain a dot or
row-dot implementation.

## Types

- `ggml.h` declares 44 types; **all 44 are mentioned** somewhere in a Goose source. Mention is not implementation - this
  is the same trap P2-01 exposed, so the second pass looked only at dot implementations.
- Types that appear next to an actual dot/row-dot implementation: `f32`, `iq4`/`iqs4`, `iq1_m`, `q8_1`, `q4_K`, `q6_K`,
  plus `f16`/`bf16` in CUDA paths.

## What that means

The K-family dots (Q4_K, Q6_K) have both CPU and CUDA Goose implementations with separate gates (160/160, 240/240,
7/7 and 6/6 mutations) - those are the ones the Apex profile executes. IQ coverage exists in part (`iq4`, `iq1_m`).
**Not shown implemented**: TQ, FP4 and the legacy block types.

## Why this is not a completion claim

The acceptance criterion asks for every active family to have Goose dot/dequant/routed tests and **zero calls to replaced
native arithmetic**. Neither is established for the uncovered families: there is no test corpus for them here and no
liveness evidence that they are off the Apex path. So P1-06 remains IN_PROGRESS with a concrete list of what is covered
and what is not, which is more useful than a percentage.

## Next concrete step for this item

For each uncovered family: establish whether it is reachable by any retained architecture. If not, record it as explicit
non-coverage (the criterion allows unsupported coverage to be declared). If yes, it needs a Goose implementation plus a
bit-exact corpus, which is stage-2 work rather than a single probe.
