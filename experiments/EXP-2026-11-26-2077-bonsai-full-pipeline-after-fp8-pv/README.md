# EXP-2026-11-26-2077 — Full cumulative Bonsai profile after FP8 PV

**Diagnostic, no runtime change.** Profile one unmodified EXP-1977
32K/512 request on the staged EXP-2076 package. RTX 4070, context
36,864, batch/ubatch 4096/1024, Q4_0 K/V, greedy and MTP off.
The EXP-2066 CUDA/OS Runtime capture method and analyzer are reused.
All 512 generated IDs and text match the scored EXP-2076 response.
Use 912.33 prefill / 32.74 decode tok/s and 51.545 s client time
from the unprofiled pair as the measured checkpoint. This traced
run took 51.736 s client time.

## Full request timeline

Startup ends at the server's first blocking accept. The manual
wait after readiness is outside startup and the request. Prefill
and decode are split at the first full vocabulary readback.
GPU activity is the union of traced server kernels and transfers.

| Stage, ms | Window | GPU activity | GPU gap | H2D on GPU | D2H on GPU |
| --- | ---: | ---: | ---: | ---: | ---: |
| Startup | 1875.71 | 947.09 | 928.62 | 942.83 | 0 |
| Prefill | 36015.46 | 35934.22 | 81.24 | 0.083 | 0.039 |
| Decode | 15725.33 | 14753.99 | 971.35 | 0.296 | 20.089 |

Startup makes two allocations for 7,481,217,024 B model and
1,720,400,640 B workspace. The four startup H2D transfers total
7,481,331,712 B. No further CUDA allocations or CUDA Graph
launches are observed during the request. The traced process
uses 8,944 MiB VRAM after the request, 8,002,616 KiB VmHWM and
zero swap. The unprofiled checkpoint reports 8,932 MiB after
the request. Profiling overhead was not measured separately.

## Kernel budget and next priorities

| Prefill kernel group | EXP-2066, s | EXP-2077, s |
| --- | ---: | ---: |
| PQ2 projections and activation packing | 24.476 | 24.032 |
| Attention including staging and merge | 8.443 | 5.982 |
| GDN, decay and convolution | 2.810 | 2.799 |
| Other kernels | 3.130 | 3.121 |

These are GPU kernel duration sums. The two profiles cover different
accumulated builds and are diagnostic rather than a paired speed
claim. Do not assign the whole Attention difference to FP8 PV or add
measured group deltas to each other as guaranteed request savings.

The largest kernel now is `bonsai_pq2_half_full_5120` at 13.294 s
across 7,440 calls. The full FFN down half2 entry follows at 5.459 s.
FP8 QK/PV takes 3.822 s in 272 launches. The generic PQ2 partial
accumulator now takes 0.722 s in 368 calls. The prior 6144-width
candidate removed most of the generic dispatch, so work focused
only on the remaining tail has a smaller measured ceiling.
Prioritize a change that can accelerate the main PQ2 projection
families, then score it on the full request.

Host CUDA API spans cover 35.925 s concurrent with GPU activity
in prefill. Only 52.51 ms of CUDA-API time falls in GPU gaps. The
GPU carries 662,460 B H2D during prefill. Long synchronous CPU calls
mostly overlap queued GPU work and must not be counted as extra
device transfer time. Process CPU counters report 42.92 s user
and 10.31 s system across the request, with zero major page faults.

Decode still has a separate opportunity. Of its 971.35 ms of GPU
gaps, 593.02 ms overlap CUDA calls on the main thread. Device work
is dominated by 8.875 s of matrices and packing and 4.321 s of
Attention. All 511 decode logit copies consume 20.09 ms of GPU
transfer time for 507,566,080 B. Consider GPU-side selection or
decode submission changes against measured gap time. Prefill has
less than 82 ms of traced GPU gaps, so eliminating every such gap
cannot close the current prefill difference.

## Reproduce

Follow EXP-2066's full capture and use the staged EXP-2076 package.
The measured configuration uses a 90-second collection duration
and periodic CUDA flushing. Export Nsight to `trace.sqlite`, retain
the markers, response and process snapshots, then run:

```bash
python3 experiments/EXP-2026-11-25-2066-bonsai-full-pipeline-profile/source/analyze.py \
  PROFILE_DIR EXPECTED_2076_RESPONSE_JSON
```

[Measured stage attribution and trace checksum](data/attribution.json).
