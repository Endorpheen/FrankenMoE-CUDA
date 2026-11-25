# EXP-2026-11-26-2089 — Phase probe for active Bonsai half2 FFN down

**Diagnostic.** The instrumented entry is bit-exact but materially
perturbs performance. Its phase shares are directional evidence only.
The hardware-counter capture was unavailable. The deployed and
cumulative runtimes are unchanged.

## Procedure

Use the EXP-2086 image and its active 17408-to-5120 half2 down
kernel, which occupies about 5.3 s of the full 32K/512 prefill trace.
Append a diagnostic clone with `clock64` reads around six existing
phase boundaries. Lane 0 in 40 of 640 CTAs records one even K step
and its adjacent odd step. Sample different pairs of K steps across
the tiles. Add no barriers and retain the same fused producer,
tile dimensions and FP16/FP32 operation order. Each timing is an
elapsed span observed by one warp and includes scheduler effects
and instrumentation overhead. Copy issue can overlap later work.

Four actual PQ2_0 down matrices, real Hadamard signs, seeded
gate/up buffers and 1024 tokens. Include the fused activation
producer in the CUDA event timing. Medians use eight observations
per arm in balanced order. Each complete output matches bitwise.

| Tensor | Control, ms | Instrumented, ms | Elapsed increase |
| --- | ---: | ---: | ---: |
| Layer 0 down | 3.14026 | 4.92851 | 56.95% |
| Layer 21 down | 3.12738 | 4.92846 | 57.59% |
| Layer 42 down | 3.01360 | 4.53734 | 50.56% |
| Layer 63 down | 3.00800 | 4.44277 | 47.70% |

SM89 compiler allocation rises from 79 to 146 registers per
thread, with 27,648 B shared memory and no spills in both entries.
The instrumented kernel's occupancy and instruction scheduling can
differ substantially from the control.

## Observed clock shares

Ranges span the four matrices. Each matrix's phase share sums
the observed cycles across its 40 selected CTAs, separately by
parity. These are mutually exclusive intervals in the instrumented
observations. They are not percentages of the full request or
wall time, and should not be scaled by the 5.3 s trace entry.

| Interval | Even K step | Odd K step |
| --- | ---: | ---: |
| Weight reads and packing of code bytes | 26–28% | 1.8–1.9% |
| Activation copy issue | 8–9% | 13–14% |
| PQ2 operand expansion and shared writes | 3.8–4.1% | 4.9–5.5% |
| Copy wait and first barrier | 12.8–14.8% | 17.8–21.5% |
| Fragment loads, MMA and FP32 accumulation | 39.9–42.4% | 54.0–56.7% |
| Final barrier | 5.2–5.9% | 4.3–4.5% |

A separate attempt to collect LaunchStats, SpeedOfLight,
WarpStateStats and SchedulerStats on one unmodified launch with
Nsight Compute 2024.3.2 returned `ERR_NVGPUCTRPERM`.
The driver's `RmProfilingAdminOnly` is 1. No hardware counters
were captured. GPU clocks and caches were left uncontrolled for
that attempt. Driver permissions were left unchanged.

## Next hypothesis

Test scheduling two adjacent K steps as one unit, staging both
halves before issuing MMA and preserving FP16 partial boundaries
and FP32 accumulation order. The current kernel already keeps raw
PQ2 bytes from the even step for the odd step. A new component
must reduce barrier or fragment staging work beyond that reuse.
Precompute the cost of the larger shared working set and use a
resource-aware tile design. A lower barrier count can lose to
lower residency, so use the uninstrumented full producer and
projection as the component control. The clock shares cannot
predict the gain.

The unchanged EXP-2086 scored reference is 1015.48 prefill /
32.59 decode tok/s and 8,932 MiB process VRAM. Saved PrismML is
1076.60 / 47.63 tok/s at 32K/512. This experiment runs only
components. Candidate request times and process memory are unmeasured.

## Reproduce

Build the isolated image from the EXP-2086 combined PTX and the
EXP-2071 OpenCL source, with Clang 18 and CUDA ptxas for SM89.

```bash
exp=experiments/EXP-2026-11-26-2089-bonsai-ffn-down-phase-budget
out=build/bonsai-ffn-down-phase-budget-2089
check=experiments/EXP-2026-11-26-2085-bonsai-ffn-down-panel/source/check.py
bash "$exp/source/build.sh" "$out" build/bonsai-q8-paired-selector-2086 build/bonsai-cumulative-half-unpack-2071/pipeline.cl
OPENBLAS_NUM_THREADS=1 python3 "$check" CORE_DIRECTORY INVENTORY "$out/pipeline.cubin" clock
```

[Component comparison](data/component.json), [raw CTA counters](data/clock-samples.jsonl),
and [build and profiler receipt](data/build.json).
