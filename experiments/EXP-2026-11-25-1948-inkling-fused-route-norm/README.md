# EXP-2026-11-25-1948: Reuse the common MoE route normalization in Inkling

**Verdict: correctness proven on 8192/256; inconclusive as a
throughput checkpoint.** Keep EXP-1946 as the runtime baseline.
Retain this local candidate for routing work.

## Mechanism

The existing Inkling graph gathers the probabilities for the
selected expert IDs, sums them, clamps the denominator, divides
and applies a scale of eight through separate nodes. The candidate
binds the aggregate MoE node directly to the shared
`fg_q35_moe_norm_node` used by the other resident path.
The node takes the same unbiased probabilities and selected IDs.
The original ranking, tie behavior, expert ordering and cache
policy are preserved. The unused intermediate subgraph is no
longer reachable from the MoE output. Enable the original chain
with `I2_FUSED_ROUTE_NORM=0`. Only the isolated Inkling host
object changes. The GPU images and common implementation are
reused from EXP-1946.

## Paired 8192/256 measurement

RTX 4070, context 36864, batch and ubatch 4096, Q4_0 KV,
736 MiB activation arena, wide vectors and 6720 MiB expert
limit. Same binary in B–A–A–B order. Each fresh process
handled one first and one repeated request, with prompt
recomputation and equal state fingerprinting.

| Paired mean | Original chain | Shared node |
| --- | ---: | ---: |
| First prefill, tok/s | 277.00 | 276.42 |
| Repeat prefill, tok/s | 280.24 | 280.71 |
| First decode, tok/s | 18.275 | 18.616 |
| Repeat decode, tok/s | 18.362 | 18.427 |
| Repeat decode, ms | 13887.1 | 13838.8 |
| Repeat client time, s | 43.346 | 43.248 |

Decode improved in both pairs. The first-request mean improves
1.87%, with 433 and 79 ms saved. The repeat mean improves
only 0.35%, with 82 and 15 ms saved. The first-request total
time moved in opposite directions in the two pairs. This
establishes numerical compatibility with the common route
node rather than a meaningful speed promotion.

All 17406 fingerprints per launch, all 256 IDs and text in
both requests, and expert fills and payload in both phases
match. The largest observed board peak was 11717 MiB in each
arm. Process peak RSS remained within 2 MiB. There was no
swap or swap-out. The frozen Apex checksum manifest passed;
Apex performance and the Inkling 32K/512 profile were not
measured.

## Reproduction

Apply `source/inkling-backend.c.patch` to the local EXP-1943
Inkling source. Use the build helper to recompile with GCC 13
and link against the EXP-1946 archive. Use the exact EXP-1944
8192/256 input fixture and launch options in `results.json`.
See `inputs.sha256` for artifact identities.
