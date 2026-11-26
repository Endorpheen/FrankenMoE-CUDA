# EXP-2026-11-26-2103 — Host and queue audit of accumulated Bonsai decode

**Diagnostic.** One cold 32K/512 on the packaged EXP-2101 build,
with GPU choice and AVX2 enabled, RTX 4070, identical EXP-1977 raw
IDs, context 36864, batch/ubatch 4096/1024, Q4_0 K/V, greedy and
MTP off. The cold-prefill validator confirms 32,768 computed input
tokens and no reuse. All 512 output IDs and text match the traced
EXP-2096 reference. No runtime, CUDA or system changes.

Use Nsight CUDA and OS Runtime events with `perf cpu-clock` samples
at 499 Hz, 4096 B DWARF stacks and CLOCK_MONOTONIC. Align the two
timelines with the pre/post UTC calibration. Exclude samples within
10 µs of GPU-gap boundaries. Restricted kernel and driver symbols
are left unresolved. There are 23,279 sampled records. Traced
throughput is 1046.13 prefill / 37.16 decode tok/s and 45.137 s
client time. Mixed profiler and run variation make these times
diagnostic. Do not replace the scored EXP-2101 mean, 1094.97 /
38.89 tok/s and 43.128 s. The trace's post-request VRAM is 8,946 MiB,
VmHWM is 8,002,524 KiB, and process swap is zero.

## CPU audit by phase

| Observed quantity | Prefill | Decode |
| --- | ---: | ---: |
| Main thread CPU samples | 15,634 | 6,860 |
| CUDA driver leaf samples | 8,734 | 4,204 |
| Runtime executable leaf samples | 25 | 13 |
| Guarded samples within GPU gaps | 63 | 110 |
| Guarded gap samples outside CUDA APIs | 29 | 2 |
| GPU gap wall time | 174.87 ms | 723.85 ms |
| Gap time overlapping host CUDA APIs | 108.85 ms | 713.31 ms |

The main-thread decode APIs overlapping GPU gaps include
436.48 ms in D2H, 228.79 ms in launch and 48.04 ms in H2D.
Only two guarded decode gap samples lie outside traced CUDA APIs,
both in profiler code. This capture does not establish a remaining
application CPU hot loop in default GPU greedy decode. Some prefill
samples show ownership bookkeeping, allocation, memmove and the
generation assembly routines, but the entire application CPU gap
budget is small. The common wait/synchronization-related leaf
samples do not expose additional profitable arithmetic to vectorize.
The findings cover this Bonsai HTTP path. They do not extrapolate
to CPU expert models, other samplers or the entire runtime.

## Readback and launch correlation

Each of the 511 device choices is paired with its actual host
D2H API using CUPTI correlation IDs. There is one full logits
readback for the first sample, then 511 × 12 B for decode.
The elapsed intervals below sum over all 511 steps.

| Measured interval | Total | Median per step |
| --- | ---: | ---: |
| D2H API entry to final choice kernel end | 8690.22 ms | 16.967 ms |
| Final choice kernel end to copy start | 2.26 ms | 4.640 µs |
| Copy service | 0.53 ms | 0.896 µs |
| Copy end to API return | 3.29 ms | 5.203 µs |

Only 6.08 ms elapses in total after final choice execution and
before host return. The 436.48 ms overlap includes gaps among
preceding device work while the API waits. It is not a readback
cost that a host mailbox can remove.

For each decode GPU gap, correlate the next measured operation with
its host API. The three bins below partition the gap union.

| Next operation's host API relative to gap start | Gap time |
| --- | ---: |
| Already returned | 649.05 ms |
| In progress | 20.49 ms |
| Starts later | 54.30 ms |

Only 6.69 ms of the last bin lies strictly before the next API starts.
Most of those 6.69 ms precede H2D. An API that returned before the
gap demonstrates earlier host submission, but does not identify
hardware or driver scheduling causes. The largest observed
transitions are FWHT to Q8 pack (100.17 ms aggregate) and Q8 pack
to PQ2 MMVQ (84.82 ms). All transition sums are diagnostic.

The scored decode gap to PrismML is about 2.51 s. Even removing
the full traced 0.724 s gap would not close it with fixed kernel
durations. With kernels fixed, prioritize a scoped CUDA Graph
component that submits the existing sequence with identical
operands and dependencies, including position updates and KV
tile-count boundary handling. Measure whether graph replay changes
the gaps after host submission. Test only after a component
demonstrates correctness and a material gain. Keep AVX2 and
device choice in the cumulative profile for any subsequent A/B.

## Reproduce

Use the packaged EXP-2101 and exact EXP-1977 request with the
cold-prefill validator. Follow EXP-2098's dual capture procedure.
After exporting `trace.sqlite` and `perf.txt`, run:

```bash
exp=experiments/EXP-2026-11-26-2103-bonsai-combined-host-audit
out=build/bonsai-combined-host-perf-2103
python3 experiments/EXP-2026-11-26-2098-bonsai-host-perf/source/attribute.py "$out" --device-choice
python3 "$exp/source/decode-queue.py" "$out"
```

[Capture and workload](data/capture.json), [CPU and GPU gaps](data/host-gaps.json),
and [readback and queue correlations](data/decode-queue.json).
