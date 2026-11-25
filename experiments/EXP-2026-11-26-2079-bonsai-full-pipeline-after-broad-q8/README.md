# EXP-2026-11-26-2079 — Full Bonsai profile after broad Q8

**Diagnostic.** One request on the staged EXP-2078 profile, with all
64 gate/up layers in Q8. No runtime, image or stable-profile changes.
RTX 4070, raw EXP-1977 input IDs, context 36864, batch/ubatch 4096/1024,
Q4_0 K/V, greedy and MTP off. All 512 output IDs and text match EXP-2078.
The traced run reports 963.77 prefill / 32.44 decode tok/s and 49.775 s
client time. The unprofiled checkpoint reports 962.01 / 32.50 tok/s and
49.806 s. Saved PrismML reports 1076.60 / 47.63 tok/s and 41.174 s.
Prefill parity still requires saving 3.626 s against the unprofiled run.

## Timeline

Boundaries and interval unions follow EXP-2066. GPU gaps are time
outside traced activity for this server. Startup ends at the first
blocking accept and excludes the manual wait for the request.

| Stage, ms | Window | GPU activity | GPU gap | H2D service | D2H service |
| --- | ---: | ---: | ---: | ---: | ---: |
| Startup | 2403.32 | 897.08 | 1506.24 | 893.35 | 0 |
| Prefill | 34028.90 | 33950.93 | 77.97 | 0.081 | 0.047 |
| Decode | 15751.49 | 14740.23 | 1011.26 | 0.296 | 20.153 |

Startup performs four H2D transfers totaling 7,481,331,712 B. Two device
allocations cover 7,481,217,024 B model and 1,720,400,640 B workspace.
There are no in-request CUDA allocations or graph launches. Traced
post-request VRAM is 8,944 MiB, VmHWM is 8,002,064 KiB and swap is zero.
The unprofiled EXP-2078 reports 8,932 MiB and 7,457,516 KiB VmHWM.
The host had other resident GPU contexts. It had sufficient free VRAM
to run this diagnostic without changing them.

Prefill H2D moves 662,460 B. The union of host CUDA API spans covers
33.989 s, overlapping GPU activity for 33.941 s and gaps for 47.44 ms.
Synchronous H2D calls consume 21.010 s on the CPU, mostly while GPU
work is queued. Charge the service and uncovered gaps separately.
Recovering the entire 77.97 ms gap would cover only 2.2% of the
prefill parity deficit in this trace.

Decode matrix and pack kernels use 8.849 s, including 8.119 s in
`bonsai_pq2_mvq_q8_2`. Decode attention uses 4.339 s. Of the 1.011 s
gap, 631.37 ms overlaps host CUDA API calls. The 511 output-logit
copies move 507,566,080 B with 20.15 ms of GPU service. Treat host
waiting and GPU-side token selection as decode follow-ups.

## Measured prefill kernel budget

The table sums kernel durations. Differences between profiles locate
changed work and do not constitute an independent performance A/B.

| Group | Prior EXP-2077, s | Current EXP-2079, s |
| --- | ---: | ---: |
| PQ2 projections and activation packing | 24.032 | 22.226 |
| Attention including staging and merge | 5.982 | 5.846 |
| GDN, decay and convolution | 2.799 | 2.765 |
| Other kernels | 3.121 | 3.113 |

The 272 × 8 grid is the 5120-to-17408 full-batch gate/up path.
The earlier trace ran 2,976 of these projections in
`bonsai_pq2_half_full_5120` (8.250 s) and 992 in
`bonsai_pq2_mmq_int8_full_n128` (2.251 s). All 3,968 now use INT8
(9.015 s). The prior trace included 31 full 1024-token chunks and
the smaller final chunk follows separate dispatch.

| Current PQ2 family | Time, s | Share of 34.029 s trace |
| --- | ---: | ---: |
| 5120-to-17408 gate/up INT8 N128 | 9.015 | 26.5% |
| 17408-to-5120 down FP16 half2 | 5.347 | 15.7% |
| Other width-5120 FP16 full tiles | 4.952 | 14.6% |
| Width-6144 FP16 full tiles | 1.907 | 5.6% |

Within the remaining 5120-input FP16 family, grids 160 × 8 and
96 × 8 represent 10,240 and 6,144 output rows at 64 rows per block.
They account for 2.364 s and 1.457 s respectively. The 192 × 8
and 16 × 8 grids add 0.947 s and 0.183 s. The trace establishes
geometry and actual dispatch. It does not record tensor addresses.

## Next component probe

Use the existing EXP-2073 INT8 N128 tile on real 5120-to-10240 and
5120-to-6144 Bonsai matrices in the component harness with 1024 tokens.
The EXP-2078 `MATRIX` dispatch restricts INT8 by gate/up weight
identity, so those full-tile shapes still use FP16. Compare complete
pack plus projection cost and measure activation-pack reuse in the
same order as actual linked producers. Keep FP16 and INT8 packs
logically distinct when their scratch is shared. At a hypothetical
20% reduction in the two measured projection costs, the gross kernel
savings would be 0.764 s before additional pack or scheduling costs.
Record any change to attention/GDN inputs as a numerical experiment.
Check intermediate outputs and apply the existing targeted quality
policy and full 32K/512 A/B only if the component justifies it.
The 5.347 s down projection is a separate opportunity. EXP-2040
recorded no repeatable gain from the existing late-layer Q8-down path,
and EXP-2075 rejected the dense FP16 replacement at component level.

## Reproduce

Use the [EXP-2066 capture and analyzer](../EXP-2026-11-25-2066-bonsai-full-pipeline-profile/README.md)
with the staged EXP-2078 profile and a 90-second collection duration.
Export `trace.sqlite` and compare against EXP-2078 B response JSON.
The local trace directory is `build/bonsai-full-pipeline-profile-2079`.
The [attribution receipt](data/attribution.json) includes the trace
SHA-256. The [projection breakdown](data/projection-grids.json)
contains the grouped SQL query and per-grid results for both captures.
