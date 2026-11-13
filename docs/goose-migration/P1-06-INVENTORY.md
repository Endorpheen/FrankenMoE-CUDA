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

## Reachability check (2026-11-12)

Checked whether TQ/FP4/legacy types can be declared out of scope by being unreachable:

| type | referenced by a model source | references in ggml-cpu |
|---|---|---|
| TQ1_0, TQ2_0, IQ4_NL, MXFP4, Q5_1, Q4_1, Q3_K | 0 | 1 (type registry only) |

They are not mentioned by model code at all because tensor types come from the GGUF file, not from the architecture source.
So the honest conclusion is the opposite of convenient: these types are **reachable in principle** by any model whose
GGUF is quantised with them, and P1-06 cannot be narrowed by declaring them unsupported.

Consequence: finishing P1-06 means implementing the dots and building a bit-exact corpus per family. That is stage-2 work,
not a probe, and it cannot be closed inside an audit pass. Recorded so the item is neither inflated nor quietly dropped.

## Current honest status of the three open items

| item | state | what it needs |
|---|---|---|
| P2-01 | PARTIAL | permissioned regenerate+rebuild of the SILU association, or an explicit 1-ULP/25% contract |
| P1-06 | IN_PROGRESS | implementations plus corpora for TQ/FP4/legacy families |
| P1-09 | IN_PROGRESS | Goose ownership of physical allocation/OS placement/expert chunk claims |
| P1-10 | IN_PROGRESS | cache constructor, tensor allocation, full upload-copy and resource lifecycle |
