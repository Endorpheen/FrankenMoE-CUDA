# EXP-2026-11-25-1972: Freeze Inkling before Bonsai integration

Inkling optimization is paused. The comparison reference is EXP-1969,
qualified on Apex by EXP-1970. The rejected Flash experiment EXP-1971
remains available for future work. Neither production launcher changed.

| Reference | Prefill, tok/s | Decode, tok/s | Client, s |
| --- | ---: | ---: | ---: |
| Inkling first 32K/512 request | 296.244 | 22.035 | 134.113 |
| Apex second 32K/512 request after full warm-up | 3418.759 | 80.400 | 16.029 |

Both use the same accepted executable. All scored output IDs and
reference counters match their respective controls. Inkling peak VRAM
is 11735 MiB and peak RSS is 14229756 KiB for the accepted arms.
The Apex block and its separate startup and request swap accounting
are in [EXP-1970](../EXP-2026-11-25-1970-apex-q8-regression-gate/README.md).

The preserved local bundle is `build/checkpoint-inkling-exp1969-before-bonsai`.
It contains the exact executable, archive, image, host and kernel
sources, launch settings, full request, and evidence for EXP-1967
through EXP-1971. Verify `sha256sum -c SHA256SUMS` from that directory.
The tracked build recipe remains in [EXP-1969](../EXP-2026-11-25-1969-inkling-q8-packed-input/README.md).
Run the archived arguments with the frozen binary substituted for the
recorded executable path. Unset diagnostic state logging when scoring.
The independent frozen Apex reference remains EXP-1880.

## Remaining loss map

| Path | Evidence | Next measurement |
| --- | --- | --- |
| Dense Q8 attention projections | 4.426 s in the last 4096-token tile before packing, EXP-1968. EXP-1969 removed 11.77 s from the complete prefill. | Reprofile the accepted packed kernel before estimating its residual cost. |
| Full and sliding Flash | 2.727 s in the final profiled tile, EXP-1968. EXP-1971 removed 1.96 s from a paired complete prefill, but warm Apex decode lost 0.81%. | Isolate the architecture-specific kernel and requalify Apex. |
| MoE and transfers | Grouped IQ4 kernels took 1.854 s and unoverlapped H2D took 2.140 s across the same diagnostic tile. | Split expert traffic from activations and measure actual overlap. |
| Host preparation | The older diagnostic saw 7.837 s input preparation, 4.146 s residual/FFN preparation and 1.916 s convolution across prefill. | Measure the exposed host time again under the accepted Q8 profile. |
| Decode | Accepted result spends 23.19 s. Older EXP-1956 showed CPU/GPU activation boundaries, dense Q8, Flash and expert transfers. LFU, the joined bridge and parallel softcap have since reduced the budget. | Capture a new critical path and separate repeated graph boundaries from transfer service. |

The phase trace used a diagnostic 6464 MiB expert limit and predates the
accepted Q8 build. Its regions are not additive with wall time or exact
current savings. No further Inkling optimization is part of this checkpoint.
