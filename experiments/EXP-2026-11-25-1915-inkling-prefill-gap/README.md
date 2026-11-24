# EXP-2026-11-25-1915: Measure the 1024/64 Inkling prefill gap

The same frozen 1024 prompt IDs, 64 output budget, Q4_0 key and value
caches, context 2048, batch 1024, ubatch 512, CPU thread profile and
sampling options ran serially on RTX 4070 using the same
Inkling-2-Mini AD-IQ4_XS GGUF. Each server received a cold request and a
repeat, with prompt-state reuse disabled. System file cache was warm.
The decode rate uses 63 inter-token intervals. The llama.cpp reference
is local revision `4c92c5bfb7cab395af9fc0fe0939e34ae5083345`.
Goose uses the experimental checkpoint of EXP-1914. The two engines
retain their own device placement and graph scheduling.

| Runtime | Cold prefill | Warm prefill | Cold decode | Warm decode | Board peak |
| --- | ---: | ---: | ---: | ---: | ---: |
| Goose | 151.97 | 166.96 | 13.92 | 13.98 | 9835 MiB |
| llama.cpp | 296.21 | 294.60 | 13.89 | 14.44 | 11173 MiB |

All 64 output IDs match in both cold and repeated requests. There was no
process swap or system swap-out. Decode agreement here does not qualify
the 32K/512 target workload. See `comparison.json`.

## Warm prefill timeline

Separate CUDA traces used the same inputs and profiles. Align each
warm window to the first warm vocabulary readback and extend it
backward by the runtime's reported prefill duration. Records in
`paired-prefill.json` include per-engine logits sizes and window lengths.
The following interval classes are disjoint. Values are milliseconds.

| Activity | Goose | llama.cpp | Goose minus llama.cpp |
| --- | ---: | ---: | ---: |
| Kernels without data copies | 2643.5 | 899.3 | +1744.2 |
| No recorded GPU activity | 1879.0 | 303.4 | +1575.6 |
| More than one concurrent activity class | 399.3 | 0.0 | +399.3 |
| D2H only | 235.0 | 3.7 | +231.3 |
| D2D only | 0.5 | 4.8 | -4.3 |
| H2D only | 1116.4 | 2411.6 | -1295.2 |
| Total | 6273.7 | 3622.8 | +2650.9 |

The most expensive Goose kernel families are IQ4 expert projections
(1430.5 ms summed), dense Q8 (1044.0 ms), and Flash Attention (281.8 ms).
These are components of the GPU timeline. Their sums must not be added
to the disjoint table. In a separate exact-state host-phase run, the
two warm microbatches spent 230.0 ms on input preparation and 437.6 ms
on post-convolution and FFN preparation. The graph-call times also
contain GPU execution, copies and waits. See `host-phases.json`.

Of the GPU-inactive time, 1619.8 ms in Goose and 234.3 ms in llama.cpp
also have no recorded CUDA API call. Goose spends 239.4 ms in copy
calls without recorded GPU activity against 10.1 ms upstream. Explicit
synchronization alone while the GPU is inactive accounts for about
6.5 ms in Goose and 9.7 ms upstream. Summed driver API duration
includes waits during active GPU work. The remainder outside CUDA APIs
requires host-side attribution.

The two engines moved similar H2D volumes (23.62 and 24.33 GB), but
Goose overlapped some of its transfers and had less exposed H2D time.
D2H volume was 908 MB versus 81 MB. For the larger raw comparison gap,
Goose's repeated expert payload rose from 5.39 GB at 512 inputs in one
microbatch to 19.36 GB at 1024 inputs in two. The corresponding observed
unprofiled prefill gap grew from about 0.96 s to 2.66 s. Larger repeated
traffic explains part of the workload scaling. The direct 1024 trace
locates the main relative penalties in compute and GPU-inactive gaps.

## Follow-up gates

Increasing ubatch to 1024 reduced the repeated expert payload from
19.36 to 6.03 GB and raised paired warm prefill from 166.85 to
183.36 tok/s. All 2302 internal fingerprints per launch, IDs and text
matched. However, cold decode reduced from 13.88 to 13.66 tok/s and
was lower in both pairs. Warm decode was 13.89 versus 13.83 tok/s.
The board peak increased by 219 MiB and process peak RSS by about
103 MiB across the paired maxima. Do not make this the default
profile under the decode regression gate. See `ubatch-ab-results.json`.

A standalone trial of an ordered AVX2 FP32 product and FP64 accumulation
for the CPU depth-mix and RMS retained all fingerprints but did not
improve prefill (166.89 versus 166.96 tok/s in the warm comparison).
Keep focus on resident MoE/Q8 execution, repeated graph handoffs and
host work outside CUDA. The production configuration and frozen Apex
binary are unchanged. A combined-runtime Apex gate remains open.

To reproduce the trace report, use separate identical launches of the
local engines with `nsys --trace=cuda` and export both SQLite files.
The analyzer expects `profile/ub512-baseline` and
`profile/upstream-ub512`, each containing `inkling.sqlite` and the
matching `warm.json` response. Run `python3 compare-prefill-traces.py`
from the experiment directory.
