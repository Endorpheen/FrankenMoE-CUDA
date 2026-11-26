# EXP-2026-11-26-2105 — Replay the Bonsai FFN decode with CUDA Graphs

Accepted in the cumulative experimental checkpoint on top of EXP-2101.
The Bonsai HTTP backend records each layer's ten FFN launches once and
replays them in the original order on the original execution stream.
The graph uses the existing device buffers and CUDA kernels. GPU argmax
and AVX2 CPU sampling remain on their respective EXP-2101 paths.

The capture covers single-token decode with the existing Q8 matvec path
and wide normalization, from FFN normalization through residual add.
The host verifies the expected ten nodes and all 64 layer scopes. Keep
the direct path when graph facilities are unavailable or the geometry is
ineligible. On an unexpected active sequence, fail the request and disable
graph replay on later requests rather than resubmitting partial work.
The experiment changes only the Bonsai session host code. The CUBIN,
frozen PTX, common entry object, and Bonsai model host object are unchanged.

## RTX 4070, cold 32K/512, MTP off

Use the same PQ2_0 GGUF and explicit EXP-1977 input IDs, context 36864,
batch 4096, ubatch 1024, Q4_0 K/V, greedy, masked EOG and seed 123.
The fresh control uses the same executable with FFN graphs disabled.
Each arm starts a fresh server and passes the cold-prefill validator with
32768 computed input tokens and zero cached tokens. The reverse-order
pair checks the small first-pair decode gain. All four responses match all
512 token IDs and text of the saved PrismML response exactly.

| Arm | Prefill, tok/s | Decode, tok/s | Request, s | Process VRAM after, MiB | Peak RAM, MiB |
| --- | ---: | ---: | ---: | ---: | ---: |
| A, direct | 1077.20 | 38.24 | 43.842 | 8934 | 7283.12 |
| B, graph | 1081.09 | 38.52 | 43.640 | 8944 | 7282.77 |
| B2, graph | 1098.25 | 39.32 | 42.893 | 8944 | 7283.20 |
| A2, direct | 1094.14 | 39.00 | 43.113 | 8934 | 7282.79 |
| Control mean | 1085.67 | 38.62 | 43.478 | 8934 | 7282.96 |
| Candidate mean | 1089.67 | 38.92 | 43.267 | 8944 | 7282.98 |

Mean decode improves 0.78%, saving 102 ms in the decode interval. Total
client time drops 211 ms. The candidate changes decode dispatch only,
so the observed 0.37% prefill change is run-to-run variation. There was
no process swap. The historical EXP-2101 score remains 1094.97 / 38.89
tok/s. The saved PrismML reference is 1095.12 / 48.08 tok/s.

Four short HTTP cases covering greedy, unmasked EOS, probability output
and seeded temperature sampling match between arms, including the returned
probability rows. The EXP-2104 component test checked the underlying ten
operations bitwise. This experiment did not capture full-logit tensors or
repeat the inherited numerical quality gate. Apex and Inkling release
artifacts are unchanged and were not rebenchmarked. The stable Bonsai
checkpoint is unaffected. The packaged MTP CLI remains separate at EXP-2047.

## Executed GPU work and remaining budget

The diagnostic Nsight capture records 327040 graph nodes during decode
(511 steps × 64 layers × ten nodes) and zero during prefill. Per-kernel
invocation counts match the EXP-2101 trace exactly in both phases. Each run
performs one initial 993280-byte vocabulary readback and 511 12-byte
decode readbacks. GPU activity gaps total 524 ms during candidate decode,
compared with 632 ms in the saved EXP-2101 trace. The traces were captured
separately and do not replace the unprofiled pairs.

The largest measured decode intervals are PQ2 Q8 two-row matvec, 8.210 s,
and long-context Attention, 2.113 s. The first is a higher-priority target
than further launch savings. For a minimal probe, classify the 188048
matvec invocations by weight tensor, projection shape and bytes, and
compare their effective bandwidth to the saved PrismML trace before
choosing a kernel or memory-layout change. Prefill's largest entries
remain PQ2 MMQ, led by 6.786 s for `bonsai_pq2_mmq_int8_full_n128_spread`.
Kernel totals are device activity, not an estimate of removable latency.

## Reproduce

Use `source/build-http.sh OUTPUT EXP_2101_BUILD` with GCC 13, CUDA driver
headers, the EXP-2101 host sources and objects, and the checkpoint inputs
from EXP-2101. The script verifies the original CUDA image and entry
object. Set `BONSAI_HTTP_FFN_GRAPH=0` for direct submission or `1` for
graphs. Keep the saved launch arguments and all other settings identical.

[Scored pairs, settings and hashes](data/results.json) ·
[Diagnostic trace tables](data/attribution.json).
