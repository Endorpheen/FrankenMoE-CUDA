# EXP-2026-11-25-1933: Attribute the Inkling 32K decode critical path

This diagnostic profiles EXP-1932 with its Q4_0 single-query Flash
specialization and EXP-1928 memory profile. RTX 4070, context 36864,
batch 4096, ubatch 2048, a 576 MiB activation arena and a 6720 MiB
expert limit. The 32768/512 request matched all 512 IDs and text
from the unprofiled reference. The profiling response ran at
193.14 prefill and 14.43 decode tok/s, compared with 15.28 decode
tok/s without the profiler in EXP-1932. Use the trace for
attribution and the non-profiled data for throughput comparison.

## Full request, final 128 decode intervals

The delayed CUDA trace retained 462 logits readbacks. Anchor
the analysis at the end of the first and last of the final
129 readbacks. All rows below are disjoint timeline categories.

| CUDA timeline | ms per interval |
| --- | ---: |
| Kernels only | 33.263 |
| No recorded GPU work | 28.471 |
| H2D only | 5.963 |
| D2D only | 0.395 |
| D2H only | 0.184 |
| Multiple CUDA activity classes | 1.104 |
| Total | 69.380 |

Inside the 28.471 ms without GPU work, 18.050 ms have no recorded
CUDA API, 6.302 ms are inside CUDA copy calls, 3.225 ms inside
launch calls, 0.848 ms inside explicit CUDA synchronization and
0.047 ms inside other CUDA APIs. Time outside CUDA APIs includes
host code, scheduling and profiler effects. These observations
do not identify all causes or show that an interval can disappear.

The main summed kernel contributions are expert IQ4 matrices
(11.772 ms), specialized Flash (7.579 ms), dense Q8 matrices
(7.198 ms), and vector operations (5.902 ms). The two expert
plane copy sizes account for 103.31 MB per interval and 6.563 ms
of summed service. The trace records about 23.95 paired expert
uploads per token. These summed durations can overlap. Keep
them separate from the disjoint wall counts above.

## Host phase probe

A separate diagnostic binary used the same runtime archive,
context, cache and the 2048/64 request. All 64 IDs and text match
the frozen output prefix. No device profiler was active. The
first 63 decode intervals give these graph timings.

| Graph scope | Wall ms/interval | Calling-thread CPU ms/interval |
| --- | ---: | ---: |
| Allocate GGML context | 0.099 | 0.096 |
| Build graph | 0.300 | 0.299 |
| Plan allocation | 3.076 | 3.083 |
| Execute plan | 55.406 | 55.384 |
| Result and cleanup | 0.197 | 0.196 |

The broader scopes in the same probe were 24.32 ms for Attention,
35.58 ms for MoE, 1.02 ms for the CPU bridge and 2.22 ms for
logits. The host graph measurements are subsets of those scopes.
The near-equal execution CPU and wall values include driver
submission and busy waits. They do not attribute 55 ms to
useful CPU arithmetic. The 3.67 ms spent around the graph
execution is a bound on removing those particular host scopes
if dependencies, correctness and memory stay the same. It
would not close the 9.52-second full-decode gap on its own.

The remaining mechanism to test is lengthening device-resident
execution between unavoidable host route dependencies. Compare
the changed scheduling and its kernel, transfer and API intervals
with the corresponding full workload. The largest kernel family
still belongs to the expert matrices. The MoE graph is the
largest measured host phase and includes the residency decisions.

## Upstream diagnostic limitation

The delayed llama.cpp capture used the EXP-1927 fast placement
(43/49 offloaded layers, 7675.91 MiB model and 968.43 MiB compute
buffer). The process stopped during prefill and the request
returned an empty response. The kernel reported NVIDIA Xid 13
out-of-range address and Xid 43 for the process at the failure.
The failed run did not yield a comparable trace. This evidence
does not establish the root cause or reproduce the failure
without Nsight. The independently recorded unprofiled EXP-1927
measurements remain the upstream comparison.

## Reproduction

The Goose raw CUDA trace and server receipts are in
`build/inkling-exp-baseline/device-attention/inkling-decode-warp-softmax-registers/profile/full-decode`.
Use `analyze-decode.py` with that directory and its exported
SQLite, server log and cold response. See `results.json` for
the delay and duration, arguments and exact host phase measurements.
The host diagnostic scripts and build helper are in `source`.
Apply the two Python scripts to the local EXP-1921 host sources
and link against the EXP-1932 archive before running the probe.

No runtime implementation or deployment settings changed. The
frozen Apex manifest passed. No Apex performance test ran here.
