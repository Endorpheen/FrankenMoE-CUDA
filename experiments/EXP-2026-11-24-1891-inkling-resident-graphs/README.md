# EXP-2026-11-24-1891 — Reuse the resident graph executor in Inkling

**Accepted diagnostic candidate.** Use the existing duration-aware tensor
arena and single-result graph plan for Inkling MoE. A second change groups
the loop-controller hidden projection and the gain/forget projections
into one resident graph with a combined output. These are host-harness
changes; the shared graph executor and the EXP-1890 matrix code are reused
as built. Attention, convolution, mixing and other graph boundaries
are unchanged.

RTX 4070, Inkling AD-IQ4_XS, context 128, 24 input IDs, two microbatches
(16 and eight), 17 greedy outputs and 16 measured decode intervals. Same
binary, EXP-1890 kernels, EXP-1888 KV path, and two-plane expert bank.
A-B-B-A with `I2_RESIDENT_PLAN=0/1`. All 17 output IDs, 57 residual/logit
fingerprints, cache counters and Attention transfer counts matched.

| Arm | Prefill seconds | Decode tok/s | Graph evaluations |
| --- | ---: | ---: | ---: |
| A1 | 3.444 | 5.109 | 7793 |
| B1 | 3.102 | 5.316 | 4337 |
| B2 | 3.123 | 5.269 | 4337 |
| A2 | 3.440 | 5.118 | 7793 |

Both pairs improve. Mean prefill 6.97 → 7.71 tok/s (+10.6%).
Mean decode 5.11 → 5.29 tok/s (+3.5%). Peak RSS and the observed
GPU allocation stay essentially level and system swap-out is zero.
The separate MoE-only trial also preserved tokens and fingerprints.
These figures apply to the short diagnostic workload only.

## Trace

Compared with EXP-1890's diagnostic trace, decode synchronizations
fall from 24848 to 7952, D2H copies from 25616 to 11024, and D2H
payload from 404 MB to 148 MB. Prefill synchronizations fall from
3105 to 993 and D2H payload from 588 MB to 203 MB. Decode H2D payload
remains around 3.08 GB over 16 tokens. CUDA API intervals can overlap
device intervals. No production 32K/512 result is claimed.

The next step is residency across the boundaries between subgraphs
and the exact-data transfer path for expert fills and routing.
Apex EXP-1880 is the full-model regression reference.

## Reproduce

Apply `source/probe.c.patch` and `source/prefill.h.patch` to the
EXP-1889 diagnostic harness and link with the EXP-1890 archive.
Use its settings and add `GOOSE_CUDA_DEFER_WAITS=1`,
`GOOSE_CUDA_DEFER_MATRIX_WAITS=1`, `GOOSE_CUDA_ARENA_REUSE=1`,
and `GOOSE_ACTIVATION_LIFETIMES=1` in both arms. Use the switch
above, the exact IDs in [results.json](results.json), and one fresh
process per arm. The optional [NVTX patch](source/trace.patch) needs
nvToolsExt. See [trace results](trace-results.json).
