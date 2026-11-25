# EXP-2026-11-26-2082 — Full Bonsai profile after exact paired PQ2 loads

**Diagnostic.** One request on the staged EXP-2081 checkpoint. RTX 4070,
exact EXP-1977 raw input IDs, context 36864, batch/ubatch 4096/1024,
Q4_0 K/V, greedy and MTP off. All 512 output IDs and text match the
scored EXP-2081 response and the saved PrismML response.

The traced request reports 978.03 prefill / 32.43 decode tok/s and
49.285 s client time. The unprofiled EXP-2081 mean is 976.17 / 32.65
tok/s and 49.244 s client time. Saved PrismML reports 1076.60 / 47.63
tok/s and 41.174 s. Prefill parity requires 3.133 s of savings from
the unprofiled mean.

## Critical path

Window definitions and interval accounting follow EXP-2066.
Startup ends at the server's first blocking accept and excludes
the wait for the manual request.

| Stage, ms | Window | GPU activity | GPU gap | H2D service | D2H service |
| --- | ---: | ---: | ---: | ---: | ---: |
| Startup | 2585.62 | 1011.60 | 1574.02 | 1007.31 | 0 |
| Prefill | 33532.77 | 33451.82 | 80.95 | 0.080 | 0.039 |
| Decode | 15757.90 | 14762.28 | 995.62 | 0.296 | 20.188 |

Startup transfers 7,481,331,712 B in four H2D copies. Allocation
sizes remain 7,481,217,024 B of model and 1,720,400,640 B of workspace.
The request shows no CUDA allocations or graph launches. Post-request
VRAM is 8,944 MiB, VmHWM 8,000,952 KiB and swap zero under the tracer.
The scored EXP-2081 runs report 8,932 MiB.

Prefill transfers 662,460 B H2D. Only 50.73 ms of host CUDA API time
overlaps prefill GPU gaps. Those gaps can account for at most 2.6%
of the measured prefill deficit to PrismML. Decode has 995.62 ms
of GPU gaps, with 615.21 ms overlapping host CUDA APIs. Decoding
performs 511 full-vocabulary transfers of 507,566,080 B in total.
Their service time is 20.19 ms. The two largest decode costs are
PQ2 matrix/pack (8.883 s) and Attention (4.329 s).

## Measured prefill kernel budget

| Group, s | Prior EXP-2079 | Current EXP-2082 |
| --- | ---: | ---: |
| PQ2 projections and activation packing | 22.226 | 21.707 |
| Attention including staging and merge | 5.846 | 5.863 |
| GDN, decay and convolution | 2.765 | 2.762 |
| Other kernels | 3.113 | 3.120 |

These are GPU kernel duration sums from different diagnostic captures.
For the same 3,968 full 272 × 8 launches at 256 threads per block,
the gate/up entry switches from the original N128 to the paired-load
entry and drops from 9.015 to 8.482 s. This difference is consistent
with the preceding component test. The scored paired benefit remains
the EXP-2081 result, 0.652 s mean prefill savings.

| Current prefill family | GPU kernel time, s |
| --- | ---: |
| PQ2 gate/up, exact paired loads | 8.482 |
| PQ2 FFN down, FP16 half2 unpack | 5.356 |
| Other width-5120 FP16 PQ2 | 4.957 |
| Long-context FP8 QK/PV tile | 3.753 |
| Width-6144 FP16 PQ2 | 1.901 |
| GDN V32 MMA | 1.498 |
| FP32 fused ordered Flash | 1.340 |

## Next component probe

Evaluate an exact M128 × N128 gate/up tile using 16 warps, with
eight warp pairs covering 16 output rows each. Each CTA would reuse
the same activation panel for twice as many weight rows. Preserve
the paired PQ2 loader, integer dot products and FP32 block order.
Use the real full 5120-to-17408 geometry, which divides evenly by
128. Keep tails on the existing path.

At 92 registers per thread, the current 256-thread tile fits at most
two CTAs per SM by register count. The proposed 512-thread tile fits
one, retaining a theoretical 16 resident warps if register usage
does not rise past the limit. The current 28,416 B shared working set
would grow to 37,888 B. Confirm actual compiler allocation and spills.
Use the EXP-2081 balanced component comparison on four real weights,
including pack and strict bitwise output checks. A 5% time reduction
across the gate/up family would correspond to about 424 ms of GPU
kernel time in this trace. The effect on the request is unmeasured.

FFN down remains the second-largest individual kernel. EXP-2040
recorded no repeatable gain from the earlier late-layer INT8 down
path. EXP-2075 rejected the dense FP16 replacement at the component
stage. EXP-2080 remains a separate candidate for some GDN projections,
with its quality observations recorded separately.

## Reproduce

Use the [EXP-2066 capture method and analyzer](../EXP-2026-11-25-2066-bonsai-full-pipeline-profile/README.md)
with the staged EXP-2081 package and a 90-second collection duration.
Export `trace.sqlite` and compare against EXP-2081 B response JSON.
The local trace directory is `build/bonsai-full-pipeline-profile-2082`.

[Attribution and trace checksum](data/attribution.json).
[Gate/up dispatch query and counts](data/gate-up-dispatch.json).
