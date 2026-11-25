# EXP-2026-11-25-2066 — Full Bonsai pipeline profile after cumulative MMQ

**Diagnostic, no runtime change.** One complete unmodified EXP-1977
32K/512 request against staged EXP-2065, RTX 4070, context 36,864,
batch/ubatch 4096/1024, Q4_0 K/V, greedy and MTP off. Nsight traces
CUDA APIs, GPU memory activity and OS Runtime calls over 0.1 ms.
All 512 IDs and response text match the scored EXP-2065 candidate.
The observed profiled client time is 54.686 s. The scored reference
remains 845.33 prefill / 32.67 decode tok/s at 54.430 s client time.

## Critical path

Startup uses the server launch marker through its first blocking
accept. Idle time between server readiness and the manual request
is excluded. Prefill runs from the client request marker through
the first complete vocabulary readback. Decode ends at readback 512.
The trace has a validated vocabulary kernel and readback for each
output. GPU activity is the union of server kernels, copies and
memsets. GPU gaps refer to the traced server's timeline.

| Stage, ms | Window | GPU activity | GPU gap | H2D on GPU | D2H on GPU |
| --- | ---: | ---: | ---: | ---: | ---: |
| Startup | 1912.51 | 979.49 | 933.02 | 975.50 | 0 |
| Prefill | 38942.14 | 38860.24 | 81.90 | 0.077 | 0.040 |
| Decode | 15748.97 | 14759.44 | 989.52 | 0.296 | 20.064 |

The response tail after the last GPU readback is 5.80 ms. The
diagnostic server timers report 38.913 s prefill and 15.749 s
decode. Process VRAM sampled after the request is 8,944 MiB
and VmHWM is 8,000,456 KiB with tracing. Swap remains zero.
Use the scored EXP-2065 memory observations for performance
comparison. Profiler overhead was not separately measured.

### Kernel budget

| Kernel group, ms | Prefill | Decode |
| --- | ---: | ---: |
| PQ2 projections and activation packing | 24475.95 | 8893.50 |
| Attention including staging and merge | 8443.44 | 4320.32 |
| GDN, decay and convolution | 2810.32 | 447.44 |
| Other kernels | 3130.15 | 1077.81 |

These are duration sums, not exclusive wall-time allocations.
The largest prefill entries are specialized FP16-partial MMQ at
width 5,120 (13.319 s), fused Flash (7.610 s), and specialized
MMQ at width 17,408 (5.523 s). Generic partial-accumulator
dispatch still takes 2.847 s in 2,352 calls. Most of it is in
80×8 launch grids (2.387 s). The GGUF has 64 PQ2 matrices of
shape 6,144×5,120; these are a plausible source of much of that
group, but the trace does not expose kernel parameters.
Confirm the shapes before changing dispatch.

### Host, copies, synchronization and memory

- Startup performs four H2D copies covering 7,481,331,712 B.
  Two CUDA allocations hold 7,481,217,024 B model payload and
  1,720,400,640 B workspace. The trace shows no new device
  allocation during prefill or decode. File mapping and CPU init
  are in the startup window; OS Runtime calls do not resolve
  the full filesystem or host preprocessing time.
- Prefill submits 56,085 kernels and 104 H2D copies. Their host
  H2D API calls sum to 24.280 s, but the GPU transfer activity
  itself is only 0.077 ms for 662,460 B. The union of prefill
  CUDA APIs overlaps GPU activity for 38.851 s, leaving only
  51.60 ms of CUDA-API time in GPU gaps. Large synchronous
  call durations are mostly queue or synchronization wait in
  this workload and cannot be added to GPU compute.
- Decode submits 837,018 kernels and reads 507,566,080 B of logits
  across 511 copies. GPU D2H service is 20.064 ms. Host D2H API
  time sums to about 9.80 s and overlaps queued work. The
  989.52 ms traced GPU gap includes 611.65 ms when the main
  thread is inside CUDA APIs. Investigate decode synchronization,
  sampling and launch organization there rather than assuming
  that copying the final logits takes seconds on PCIe.
- No graph launches or graph-node kernels are recorded. With less
  than 82 ms of traced GPU gaps, a graph-only prefill rescue is
  bounded for this request. A decode graph or GPU sampling probe
  addresses a different, larger gap and needs its own A/B.
- Server process CPU accounting across the request uses 45.55 s
  user and 10.61 s kernel CPU time at 100 ticks/s. It may exceed
  single-thread wall time and overlaps the GPU timeline. It is
  not a profile of individual driver spins. No major page
  faults are observed in this request window.

The largest measured steady-request cost remains PQ2 MMQ, followed
by long-context attention and decode projections. Optimize the
cumulative MMQ path first and score any resulting combination on
full 32K/512. Track startup separately for latency-to-ready and keep
host/device gaps in the next full profile.

## Reproduce

Run the staged EXP-2065 package under:

```bash
nsys profile --trace=cuda,osrt --osrt-threshold=100000 --cuda-memory-usage=true \
  --cuda-graph-trace=node --sample=none --cpuctxsw=none --duration=105 \
  --cuda-flush-interval=1000 --kill=sigterm --output=PATH/cuda \
  PATH/package/frankenmoe bonsai PATH/model.gguf 19197
```

Record UTC-nanosecond markers `launch-ns`, `request-start-ns` and
`request-end-ns`. Keep the server process stat and memory snapshots
before and after the single request, along with `client-s`,
`after-vram`, and `response.json`. Export Nsight to `trace.sqlite`.
Run `source/analyze.py PROFILE_DIR EXPECTED_2065_RESPONSE_JSON`.

[Machine-readable stage attribution and trace checksum](data/attribution.json).
