# EXP-2026-11-26-2098 — Bonsai host work in GPU gaps

**Diagnostic.** One cold 32K/512 on EXP-2096, RTX 4070. Same GGUF,
raw EXP-1977 input IDs, context 36864, batch/ubatch 4096/1024,
Q4_0 K/V, greedy and MTP off. Cold prefill passes and all 512 output
IDs and text match EXP-2096's scored response. No runtime changes.

Capture Nsight CUDA and OS Runtime events with `perf record`
attached to the serving process, sampling `cpu-clock` at 499 Hz
with a 4096 B DWARF stack. Translate samples from CLOCK_MONOTONIC
to trace time using adjacent monotonic and UTC reads before and
after the request. The two measured offsets agree, with 80 ns
brackets for the local clock reads. Nsight timestamp alignment can
introduce separate uncertainty, so exclude samples within 10 µs
of a GPU gap boundary. The first and last full vocabulary readbacks
bound the decode region. The sampled thread is the CUDA submission
thread. Other threads are counted separately.

The run captures 22,337 CPU samples and reports 1095.65 prefill /
37.52 decode tok/s and 43.551 s client time under profiling. Peak
process RSS is 8,000,528 KiB and post-request VRAM is 8,944 MiB
with no process swap. Kernel address maps are restricted. Frames
without kernel symbols stay unattributed. System settings were
left unchanged. Use EXP-2097's unprofiled comparison for scored
rates: 1094.19 / 37.82 for Goose, 1095.12 / 48.08 for PrismML.

## Timeline evidence

The GPU gap is the complement of the union of traced kernel,
copy and memset intervals in each phase. API overlap is measured
wall interval, not CPU time or a causal attribution.

| Gap interval or overlap | Prefill, ms | Decode, ms |
| --- | ---: | ---: |
| All GPU gaps | 89.97 | 982.51 |
| Overlap with CUDA APIs | 61.36 | 605.91 |
| Overlap with cuLaunchKernel | 18.15 | 186.25 |
| Overlap with cuMemcpyDtoH | 0.82 | 400.50 |
| Overlap with cuMemcpyHtoD | 32.05 | 19.16 |
| Overlap with cuMemsetD8 | 10.34 | 0 |
| Outside traced CUDA APIs | 28.61 | 376.60 |

During decode, the 511 full vocabulary transfers still take
about 20 ms of GPU copy service. The 400.5 ms overlap instead
measures idle trace time occurring within the host readback calls,
which include waiting and driver behavior. Removing the copies
alone does not imply saving the whole overlap.

## Sample evidence

The main thread has 14,920 prefill and 6,794 decode samples.
Only 22 prefill and 258 decode samples lie in the interior of
a traced GPU gap after the boundary guard. An additional 12
prefill and 227 decode samples inside gaps are too close to
their boundaries for this attribution. The guarded interiors
cover 54.91 and 514.85 ms, respectively. All counts below are
samples from the decode gap interiors.

| CUDA API attribution | Samples |
| --- | ---: |
| Outside CUDA APIs | 177 |
| cuMemcpyDtoH | 61 |
| cuLaunchKernel | 16 |
| cuMemcpyHtoD | 4 |

Outside CUDA APIs, 136 samples land in `gg_function_368`,
17 in `gg_index`, 22 in `__memmove_avx_unaligned_erms` and two
in unresolved driver code. In the actual ELF, the sampled
`gg_function_368` loop calls `gg_index` for the candidate and
the incumbent, compares their floats and updates the winner only
on strict greater-than. This is the emitted scan of
`sm_greedy` in `goose/sampling/sampler-chain-full.goose`.
The CPU evidence localizes a substantial part of work outside
CUDA APIs during GPU gaps. It does not allocate all gap time to
this scan or establish the savings from eliminating it.

## Next probe

Reuse EXP-2034's device argmax on the ordinary Bonsai greedy
path. Match EOS masking and the first-ID tie rule and keep the
host path for probability reporting and other sampler settings.
Probe only the supported route and compare candidate output IDs
with the full HTTP 32K/512 oracle before scoring. The measured
0.983 s of all decode gaps bounds the opportunity to remove them
without changing device work. The current 2.885 s decode gap to
PrismML still needs GPU-side improvements. Prefill has only
90 ms of measured GPU gaps, so continue profiling its PQ2 and
Attention cost independently.

The raw captures are local in `build/bonsai-host-perf-2098`.
Export the Nsight report as `trace.sqlite`, use
`perf script --ns -F comm,pid,tid,time,event,ip,sym,dso` to
create `perf.txt`, and run `source/attribute.py CAPTURE_DIR`.
Keep the cold-prefill validator receipt and the two clock
calibration records with the same request.

[Host-gap and sample data](data/host-gaps.json),
[CUDA timeline attribution](data/attribution.json),
and [workload and build receipt](data/capture.json).
