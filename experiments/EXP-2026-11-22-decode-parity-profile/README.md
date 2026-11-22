# Full decode gap and fast completion without probability reporting

This checkpoint uses the unchanged
[EXP-1694 executable](../EXP-2026-11-22-1694-prefill-vram-allocation/README.md).
Apex 35B Q4_K_M, 32,768 input / 512 output tokens, 4,096/4,096 batches,
Q4_0 KV, and full host registration are retained. The Goose GPU
cache is 6,290 MiB and its 210 MiB temporary workspace is cold-aware.

## Full, warmed Nsight differential

Both runtimes warm with the original request and capture the second
full request. The complete decode GPU timeline is bounded by the
first decode GDN event and final logits copy. Each window contains
511 evaluated decode steps. Coverage is device activity and CUDA
API/OSRT timings. This Nsight setup did not support CPU IP/backtrace
or context-switch sampling. Cross-runtime generated text differs.

| Decode window component, ms | Goose | Hybrid | Goose excess |
|---|---:|---:|---:|
| Total | 15,796.102 | 8,452.662 | 7,343.440 |
| GPU busy union | 9,235.501 | 6,512.834 | 2,722.667 |
| GPU idle | 6,560.600 | 1,939.827 | 4,620.773 |
| Vector attention exclusive | 2,467.729 | 1,218.477 | 1,249.252 |
| Pointwise/GLU exclusive | 753.957 | 138.850 | 615.107 |
| MMVQ exclusive | 2,804.992 | 2,484.396 | 320.596 |
| GDN exclusive | 350.866 | 56.156 | 294.710 |
| RMSNorm exclusive | 362.369 | 123.781 | 238.588 |
| Bulk H2D exclusive | 1,434.164 | 1,764.149 | -329.985 |

Kernel/transfer rows explain parts of the busy union. Do not sum
CPU API durations with those rows. The largest Goose idle category
is 3,916.980 ms between the logits D2H and the next activation H2D
outside CUDA APIs. This interval includes CPU work and cannot be
entirely assigned to the sampler from the trace alone. Goose's
larger expert bank also moves less bulk H2D in this profile:
34.72 GB versus 40.51 GB for the hybrid.

## Same-process probability differential

A full request warms the same server. The next two full requests
run with `n_probs=0` and `n_probs=5` respectively. Sampling settings,
prompt, model and executable are unchanged. Both measured requests
report 32,768 input, 512 output, and zero reused input tokens.

| Measured response | Prefill tok/s | Decode tok/s | Prefill s | Decode s | Client s |
|---|---:|---:|---:|---:|---:|
| No probability rows | 3,009.67 | 39.75 | 10.888 | 12.856 | 23.834 |
| Five probability alternatives | 3,040.62 | 33.73 | 10.777 | 15.148 | 26.039 |

The 512 token IDs and response content match. Peak process VRAM is
8,766 MiB and process swap is zero. System swap-out was 68 pages
during startup and zero in both measured requests. The separate
two-process observation was 39.09 vs 34.25 tok/s, also with identical
tokens and equal SLRU counters. That comparison had unequal system
swap-out and is secondary to the single-process measurement.

The reporting mode adds 2.293 s decode here. This includes both
report generation and the preparation/copying of logits for it.
Keep 39.75 tok/s as the fast measured configuration for clients
that do not need probability rows. It is a retained checkpoint.
The original five-alternative output remains the parity workload.
Moving reporting work across the token boundary must preserve
all returned rows and include queue drain in end-to-end timings.

## Reproduction

Local binaries, exact requests, Nsight recordings, SQLite exports,
raw responses and sample logs are preserved under
`build/exp1694-prefill-workspace-budget`. The full profiles are
in `decode-profile-full-fixed`, and the same-process comparison
is in `probability-cost-same-process`. The fast profile's request
is `request-no-probabilities.json`, with its own checksum in
`NO-PROBABILITIES-SHA256SUM`. Keep the same runtime option profile
as EXP-1694 and use that request in the ordinary harness.
