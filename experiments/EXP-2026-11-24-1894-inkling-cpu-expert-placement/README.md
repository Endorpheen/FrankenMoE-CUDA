# EXP-2026-11-24-1894 — Attribute Inkling placement and execution differences

**Scalar CPU placement rejected.** The first direct transplant of the
placement idea moves the two indexed IQ4 projections of one Inkling
physical layer (layer 4) to Goose's existing CPU path. Other layers
and the router remain on their previous CUDA paths. It uses no external
model runtime. It reduces expert uploads but is much slower.

## Same Inkling request on upstream and Goose

RTX 4070, same local AD-IQ4_XS GGUF, 24 identical prompt IDs,
65 greedy outputs, effective context 256, F32 KV, physical microbatch 16
and one CPU thread. Upstream rounds requested context 128 to 256.
Upstream warms the server for one identical request before the scored
request; Goose diagnostic constructs one sequence per process with the
model pages already warm. Upstream reports 65 predicted tokens in its
decode timing. Goose reports 64 measured intervals after its first
output. These are comparable workloads with distinct timers and
placement. Both token sequences are saved; they diverge at output
token 8, so later expert routing is not a controlled equality.

| Runner | Prefill tok/s | Decode tok/s | Peak process VRAM MiB | Peak RSS GiB |
| --- | ---: | ---: | ---: | ---: |
| Official upstream normal fit | 21.53 | 16.15 | 9276 | 14.31 |
| Official upstream near Goose VRAM | 14.54 | 11.71 | 7690 | 14.31 |
| Goose diagnostic | 7.48 | 5.67 | 7578 | 10.70 |

No swap-out in these three scored measurements. Upstream margin changes
768 → 2432 MiB, and actual CUDA model storage 8900.63 → 7321.13 MiB.
The complete Inkling file was resident in the OS page cache at the
fincore checkpoint (3661313 pages); the Goose scored run showed zero
major faults. The near-VRAM upstream RSS is mostly file-backed
(14351444 KiB), as is Goose (11130780 KiB in the peak sample).
Lower process RSS does not establish lower system-wide cache residency.
Apex already warms its whole mmap and can register its expert ranges;
this Inkling diagnostic does not yet perform the same startup policy.

## Decode attribution

The scored near-VRAM cases take about 85.4 ms and 176.3 ms per
reported decode token/interval. In separate diagnostic traces,
Goose's 64-step decode window is 11.533 s. Total visible GPU activity
union is 2.393 s, or 37.4 ms/step. Expert and other H2D transfers
total 8.460 GB, or 132 MB/step. H2D DMA time is 660 ms, about
10.3 ms/step. The host spends about 20.0 ms/step inside blocking H2D,
20.2 ms/step in D2H and 10.4 ms/step in context synchronization.
Those API times overlap GPU work and are not independently additive.
The unaccounted host intervals are not all CPU execution.

The upstream near-VRAM request has 176.4 MB H2D total, versus
15.346 GB for Goose across prefill and decode. Its approximate decode
subwindow has about 129 MB H2D. The upstream trace shows 2583
CUDA Graph launches in that subwindow, while Goose dispatches
117824 individual CUDA kernels over 64 steps. Graph spans are included
in the upstream visible GPU activity union; otherwise graph nodes would
be missing from the estimate. The profiler does not isolate the
contribution of each CPU kernel. Source and tracing confirm upstream
has mixed placement and dispatched IQ4×Q8 quantized dot paths, while
Goose repeatedly streams expert planes to its device bank. Simply
eliminating 10.3 ms/step of actual DMA cannot close a gap near 91 ms.
The large target is the complete execution path: residency plus
the dependency schedule and executor overhead around the kernels.

## Isolation trial

For the short 24/17 diagnostic at context 128, gate CPU placement only
for the two IQ4 projections of physical layer 4. CPU fallback dequantizes
to FP32 and runs the scalar dot. Under this change total expert H2D
decreases 8.544 → 7.915 GB. Prefill takes 2.932 → 76.825 s and
decode 5.630 → 0.272 tok/s. All 17 token IDs match, all 57 intermediate
fingerprints differ, and swap-out remains zero. Do not promote this
path or use the matching greedy IDs as evidence of bitwise equality.

Upstream pairs IQ4_XS with Q8_K and IQ4_NL with Q8_0 in its optimized
CPU dot path. Implementing an efficient CPU path requires a separate
quantized-kernel correctness gate. In parallel, remove graph rebuilds
and breakpoints so that mixed placement, or a device-resident expert
cache, can be evaluated under a comparable schedule. Preserve the
independent Apex EXP-1880 control.

## Evidence

[Exact request](request.json), [scored and diagnostic results](results.json)
and [two matched-shape traces](matched-profile.json). The trace recipe
uses NVTX phase windows in Goose and a host timestamp bracket around
the upstream request. The upstream prefill/decode split is approximate.
Use the [Goose placement patch](source/probe.c.patch) over the diagnostic
harness with EXP-1892 options, setting `I2_CPU_MOE_MASK=0` or `0x10`.
