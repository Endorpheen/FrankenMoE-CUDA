# EXP-2026-11-24-1889 — Batch Inkling prefill projections

**Diagnostic.** Batch dense Q/K/V, post-attention projection, loop
controllers and the two-plane expert graph across prompt rows. KV and
short-convolution state update in absolute token order within each visit.
Decode uses the original single-token path.

RTX 4070, Inkling AD-IQ4_XS, context 128, 24 input IDs and 17 greedy
outputs. Both arms use EXP-1888 device Attention and two-plane SLRU.
The candidate uses 16 prompt rows and an eight-row tail. A1 is the
equivalent sequential path in the preceding binary; A2 uses the same
binary as the final candidate with batching disabled.

| Arm | Prefill seconds | Decode tok/s | Total graph evaluations |
| --- | ---: | ---: | ---: |
| A1 | 7.029 | 3.230 | 17297 |
| B1 | 5.792 | 3.227 | 7793 |
| B2 | 5.848 | 3.124 | 7793 |
| A2 | 7.056 | 3.227 | 17297 |

Mean prefill 3.41 → 4.12 tok/s (+21.0%). Without controller batching,
prefill took 6.019 s and 14129 graph evaluations. The 17 output IDs
and 57 residual/logit fingerprints match. Cache order changes with
the new schedule (21 more fills and evictions). Peak RSS around
8.75 GiB, process GPU allocation around 7.15 GiB, no swap-out.

## Architecture and execution

Apex GGUF lists 41 blocks including one NextN. Ordinary Apex uses
40 trunk layers (30 GDN and 10 attention). Inkling has 48 visits:
40 sliding-window attention and eight global attention. Both
select eight experts per visit, but Inkling's expert width is
1152 vs 512 and model width is 2304 vs 2048. Computing selected
weight bytes from tensor shapes and quant blocks gives 1.656 GB
per token for Inkling vs 0.609 GB for the Apex trunk, or 0.626 GB
if NextN is included. These are logical weight references, not
measured DRAM or PCIe bytes. Inkling also has extra loop controllers,
short-convolution state and relative-score Attention.

In a separate Nsight trace, the same batched Inkling path spends 2587
of 5682 ms of prefill in generic quantized matvec kernels. Decode
spends 2106 of 4948 ms in those kernels over 16 evaluations:
132 ms per token before other operations. Decode logs 24848
CUDA context synchronizations and 25616 D2H copies. The earlier Apex
32K/512 trace logs 1022 synchronizations and 511 D2H copies in 511
decode evaluations. Measured H2D payload is about 192 vs 14.4 MB per
decode step, respectively. These runs have different contexts and
inputs; the comparison diagnoses execution architecture, not matched
model throughput. CUDA API wait intervals overlap GPU work and must
not be added to kernel elapsed times.

Priority: a resident graph for a complete Inkling visit, with
batched Q8_0 / IQ4_XS / IQ4_NL kernels and shared CUDA dispatch.
Fewer graph calls alone leave generic matrix throughput and host
transfers on the critical path. Keep Apex EXP-1880 as the independent
full regression control.

## Reproduce

Replace the EXP-1888 diagnostic harness with `source/probe.c` and
`source/prefill.h`. Set `I2_PREFILL_BATCH=0` or `16`,
`I2_DIAGNOSTIC_EXPERT_BANK=1 I2_DEVICE_ATTN=1 I2_CHECK_STATE=1` and
`I2_TEST_DECODE_STEPS=16`. The [receipt](results.json) records input
IDs and all exact comparisons. For profiling, link `source/trace.c`
with nvToolsExt, collect CUDA and NVTX, then export SQLite and run
`source/trace-summary.py` with Inkling and Apex SQLite file paths.
Both summaries are in [trace-results.json](trace-results.json).
Full-context server qualification is pending.
