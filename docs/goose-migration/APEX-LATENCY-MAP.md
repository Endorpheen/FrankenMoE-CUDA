# Apex 32K/512 latency map

## EXP-1621 — Current gap to the hybrid at 5,920 MiB

The refreshed ranking pairs the EXP-1620 reused graph trace
with the frozen hybrid binary's second diagnostic run at
5,920 MiB. Both runs use the same Apex model and request,
RTX 4070, 4096/4096, Q4_0 K/V, 262,144 context, CPU
limits 8/16 and masks `0f0f` / `ffff`, with zero prefix
reuse. Both traces use Nsight Systems 2026.6.1 and include
all 511 decode evaluations. Hybrid executable and library
manifests match before and after profiling. The first hybrid
trace is retained with its three pages of system swap-out.

| Profiled metric | Hybrid | Goose EXP-1620 | Goose minus Hybrid |
|---|---:|---:|---:|
| Prefill, ms | 12,505.753 | 29,840.209 | +17,334.456 |
| Decode, ms | 8,633.209 | 15,948.365 | +7,315.156 |
| Prefill, tok/s | 2,620.23 | 1,098.12 | -1,522.12 |
| Decode, tok/s | 59.19 | 32.04 | -27.15 |
| Peak process VRAM, MiB | 9,156 | 8,250 | -906 |
| Peak RSS high-water, KiB | 22,520,180 | 22,089,532 | -430,648 |
| Process swap, KiB / system swap-out, pages | 0 / 0 | 0 / 0 | 0 / 0 |

The accepted [Optimized Hybrid](OPTIMIZED-HYBRID.md) target
remains 2,668 prefill and 65.38 decode tok/s at 5,632 MiB.
The profiles below are diagnostic. They use identical request
bytes and each runtime's previous response hash. The two
runtimes generate different continuations. A launch count
difference can include different routes. Per-family exposure
uses exclusive device intervals, and GPU idle uses the device
timeline complement. Concurrent work and CPU APIs remain
outside these rows. Differences from EXP-1618 are changes in
observed gap across snapshot configurations. The controlled
same-binary change for graph reuse is recorded in EXP-1620.

### Prefill remaining gap

| Operation | Hybrid, ms | Goose, ms | EXP-1618 gap, ms | Current gap, ms | Gap reduction, ms |
|---|---:|---:|---:|---:|---:|
| MMQ Q4_K | 2,367.833 | 8,377.428 | +5,856.868 | +6,009.595 | -152.727 |
| GPU idle | 1,137.673 | 4,089.827 | +2,390.777 | +2,952.154 | -561.378 |
| MMQ Q6_K | 744.883 | 2,411.027 | +1,657.972 | +1,666.144 | -8.172 |
| Flash Attention MMA | 1,421.059 | 2,949.578 | +1,554.315 | +1,528.520 | +25.795 |
| Pointwise and GLU | 489.415 | 1,649.188 | +1,156.778 | +1,159.773 | -2.995 |
| MMQ Unresolved format | 0.000 | 989.929 | +981.240 | +989.929 | -8.689 |
| GDN | 469.615 | 1,358.429 | +889.046 | +888.815 | +0.231 |
| H2D bulk | 3,473.451 | 4,162.618 | +278.089 | +689.167 | -411.078 |
| Dense and generic matmul | 88.923 | 499.564 | +413.333 | +410.642 | +2.691 |
| RMSNorm | 241.490 | 345.136 | +103.507 | +103.646 | -0.139 |
| KV FWHT | 47.130 | 145.981 | +102.868 | +98.851 | +4.017 |
| Tensor gather and blit | 362.143 | 454.864 | +117.695 | +92.721 | +24.974 |
| Other Attention | 0.000 | 60.420 | +58.055 | +60.420 | -2.365 |
| D2D | 44.475 | 89.346 | +46.191 | +44.871 | +1.320 |
| RoPE | 12.189 | 41.209 | +29.412 | +29.020 | +0.391 |
| MoE weighted reduction | 234.161 | 244.531 | +1.286 | +10.370 | -9.085 |
| MoE router | 7.714 | 16.713 | +9.080 | +9.000 | +0.080 |
| KV writes | 6.406 | 11.769 | +6.175 | +5.363 | +0.811 |
| SSM convolution | 203.458 | 204.594 | -0.180 | +1.136 | -1.317 |
| Memset | 0.230 | 0.284 | +0.050 | +0.054 | -0.003 |

### Decode remaining gap

| Operation | Hybrid, ms | Goose, ms | EXP-1618 gap, ms | Current gap, ms | Gap reduction, ms |
|---|---:|---:|---:|---:|---:|
| GPU idle | 2,158.512 | 5,228.527 | +5,435.795 | +3,070.016 | +2,365.780 |
| Vector Attention | 1,221.331 | 2,844.536 | +1,617.290 | +1,623.204 | -5.914 |
| H2D bulk | 1,706.705 | 2,436.804 | -44.580 | +730.098 | -774.678 |
| Dense and generic matmul | 148.013 | 806.078 | +650.854 | +658.066 | -7.212 |
| Pointwise and GLU | 139.808 | 558.642 | +439.429 | +418.834 | +20.595 |
| MMVQ | 2,488.074 | 2,826.385 | +329.930 | +338.311 | -8.381 |
| MoE router | 48.589 | 215.678 | +151.212 | +167.089 | -15.877 |
| RMSNorm | 124.908 | 268.112 | +148.375 | +143.205 | +5.170 |
| H2D packets up to 4 KiB | 7.572 | 79.789 | +107.363 | +72.217 | +35.146 |
| GDN | 56.952 | 111.028 | +58.932 | +54.077 | +4.856 |
| KV FWHT | 18.096 | 52.581 | +36.396 | +34.485 | +1.911 |
| SSM convolution | 21.059 | 49.676 | +30.117 | +28.617 | +1.500 |
| KV writes | 14.570 | 34.108 | +20.304 | +19.538 | +0.766 |
| RoPE | 10.824 | 28.687 | +16.456 | +17.863 | -1.408 |
| MoE weighted reduction | 29.567 | 46.885 | +17.302 | +17.318 | -0.016 |
| MMVQ pack | 184.257 | 194.954 | +48.959 | +10.696 | +38.263 |

The largest prefill arithmetic target is Q4_K MMQ. The
current aggregate identifies the family, while the retrievable
kernel launch grids preserve the exact specializations.
Dense and routed projections require matched logical M/N/K
and separate timing. In particular, the hybrid's `mul_mat_q`
count combines multiple tensor shapes and both execution paths.
The 8 KiB inter-token handoff and the bulk expert transfers
remain separate decode work.

### Q4_K MMQ with matched operand shapes

The diagnostic 4096/2 request evaluates a 4,092-column
checkpoint body in both runtimes. NVTX spans capture logical
operands and distinguish dense from routed calls. The analyzer
joins the host launch timestamps and CUPTI correlation IDs to
424 hybrid and 960 Goose MMQ kernel launches. The temporary
hybrid interposer calls the unchanged released MMQ functions.
The Goose probe uses the EXP-1620 PTX. Compare device kernel
sums per logical operation among these exact dense shapes.

| Dense Q4_K M × N × K | Operations each | Hybrid / Goose kernels per operation | Hybrid, ms/op | Goose, ms/op | Remaining gap, ms/op |
|---|---:|---:|---:|---:|---:|
| 8192 × 4092 × 2048 | 26 | 1 / 2 | 1.538 | 9.877 | +8.340 |
| 2048 × 4092 × 4096 | 40 | 1 / 3 | 0.766 | 5.100 | +4.334 |
| 4096 × 4092 × 2048 | 30 | 1 / 2 | 0.792 | 5.059 | +4.267 |

For the first shape the hybrid launches 2,048 CTAs with
`mul_mat_q<Q4_K,128,false,F32>`. Goose launches 15,872
CTAs using `goose_mmq_q4_dense_x128`, followed by 2,048
in `goose_mmq_q4_reference` for the tail. Both primary
kernels run 256 threads per CTA and have IMMA and LDSM
instructions. The runtime records 221 registers per thread
and 57,856 dynamic shared bytes for the hybrid, and 120
registers and 5,120 static shared bytes for the Goose wide
kernel. Its current 16 × 128 tile leaves much more work
fragmented across CTAs. A larger shared weight tile with
cross-warp reuse is the next candidate. Prior EXP-1611
already tested a 32-row variant whose kernel gain failed
the full-request test.

The unresolved generic `goose_mmq_matmul` in this probe
executes routed Q4_K down projection with M=2048 and K=512.
All 60 launches belong to that path. Its 20 large
N=32,736 operations average 6.254 ms across two launches,
versus 3.287 ms across one launch in the hybrid. The
remaining 20 launches serve the N=32 checkpoint tail.
Equal flattened N leaves per-expert routing distributions
open. The full trace retains the unresolved-format row
until its own launches have logical attribution.

The raw profiles and `current-map.json` are preserved in
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/parity-refresh-exp1621`.
`mmq-shapes/shape-comparison.json` preserves the exact
logical shapes, grids and kernel resources. That directory
also contains the shipped hybrid SM89 SASS and Goose SASS
assembled offline from the archived PTX.

## EXP-1620 — Reuse the long decode graphs

EXP-1620 retains the linear CUDA Graph through the next Top-K.
An 8192/32 probe compared 1,200 successive region instances.
All 13,500 copy descriptions were stable. Only 900 of 57,690
kernel nodes changed, consisting of 600 KV writes and 300
Flash Attention invocations. The replay path snapshots the
argument bytes and validates the operation order, copy
addresses and kernel parameter layouts. It sets parameters on
changed executable nodes, recaptures changed shapes that
cannot be updated and discards retained graphs on device
memory release. PTX supplies kernel parameter counts, and
both device images are byte-identical to EXP-1619.

The control and candidate use the same new executable,
5920 MiB SLRU, async expert staging, 4096/4096, Q4_0 K/V,
RTX 4070 and the same zero-prefix-reuse Apex 32K/512
request. `GOOSE_CUDA_DECODE_REUSE=0|1` selects the arm
within the cumulative experimental graph path.

| Unprofiled A-B-B-A | Capture each step | Reuse executable | Difference |
|---|---:|---:|---:|
| Median prefill, tok/s | 1,149.60 | 1,148.12 | -1.48 |
| Median decode, tok/s | 34.03 | 35.72 | +1.70 |
| Median prefill, ms | 28,503.946 | 28,540.566 | +36.621 |
| Median decode, ms | 15,017.720 | 14,304.508 | -713.212 |
| Peak process VRAM, MiB | 8,218 | 8,218 | 0 |
| Maximum RSS high-water, KiB | 21,481,204 | 21,481,668 | +464 |
| Process swap, KiB / system swap-out growth, pages | 0 / 0 | 0 / 0 | 0 / 0 |

All four runs match text, all 512 IDs, probabilities and the stop
reason, with complete model RAM residency. SLRU has 572,259
hits, 68,793 fills, 65,518 evictions and 131,964,911,616 uploaded
bytes in each arm. In the live 8192/32 check, each of the 30 steps
after warm-up reuses 40 regions with 30 changed kernel nodes.
The Q4_K and Q6_K fixtures match output bytes with 48 fills and
40 evictions per fixture, and both have zero Compute Sanitizer
errors.

### Trace confirmation

| Decode, full matched Nsight traces | Capture each step | Reuse executable | Difference |
|---|---:|---:|---:|
| Server decode, ms | 18,275.817 | 15,948.365 | -2,327.452 |
| GPU idle union, ms | 7,478.978 | 5,228.527 | -2,250.451 |
| Host calls to `cuLaunchKernel` | 993,386 | 12,656 | -980,730 |
| Full executable graph updates | 20,400 | 0 | -20,400 |
| Individual kernel node parameter updates | 0 | 15,300 | +15,300 |
| GPU CUDA kernel events | 998,476 | 998,476 | 0 |
| CUDA Graph launches | 22,990 | 22,990 | 0 |
| Time after expert copies complete and before graph submission, ms | 1,308.916 | 183.479 | -1,125.438 |
| Expert H2D device time, ms | 2,538.162 | 2,465.121 | -73.041 |
| Expert H2D overlapping GPU kernels, ms | 0.000 | 1.093 | +1.093 |
| Remaining synchronous H2D | 1,021 | 1,021 | 0 |

The 511 decode steps preserve the complete 40-region graph
per step and the distribution of 49-, 48-, 22- and two-kernel
replays. There are 56,934 async expert copies and zero
synchronous expert copies in each arm. In the replay trace,
10,261 of 10,335 miss groups finish their last transfer
before the first kernel. Only 0.248 ms between the last
independent kernel and first routed matvec still has a
required copy in flight. The reduced submission delay is
the measured effect. Additional copy/compute overlap remains
small. Nsight event tracing affects scheduling, so use the
unprofiled series for throughput. The event trace records
the external wait nodes only at capture in the reuse arm.

The host interval after logits readback and before the first
8 KiB upload of the next token remains at 1,274.503 ms of GPU
idle, versus 1,276.791 ms in the control trace. This handoff
is the largest remaining interval between identifiable host
calls in the replay trace. The trace does not split it into
sampling, probability formatting, graph construction and
off-CPU time. `profiles/latency-map-decode.json` contains
the disjoint idle accounting for both arms.

The executable, traces, probe and verification logs are in
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/decode-reuse-exp1620`.
The executable SHA-256 is
`41321543f047cab56df23398dec69bdaf64d6635e6c896392bc8c1b1b902ecbf`.

## EXP-1619 — Long decode graphs and expert staging

EXP-1619 extends the opt-in Goose graph from the full FFN through
the following layer's recurrent or attention work and device
Top-K. The graph ends before reading the IDs on the host.
RMSNorm and SSM convolution use model-owned GPU-resident weights.
The resident dense allocation adds 4,639,232 bytes. The staged
variant adds an external CUDA event before the first routed
projection. Its first ten kernels can execute before that event.
The copy stream waits for earlier readers of the cache slots.

Both arms use the same independent Goose binary, graph,
RTX 4070, Apex request and settings from EXP-1618, including
4096/4096, Q4_0 KV, CPU affinity and a 5,632 MiB cache budget.
`GOOSE_CUDA_DECODE_GRAPH=1` enables the region. The A/B switch
is `GOOSE_CUDA_DECODE_EXPERT_OVERLAP=0|1`.

| Unprofiled, 5,632 MiB | Synchronous | Staged | Staged minus synchronous |
|---|---:|---:|---:|
| Median prefill, tok/s | 1,148.69 | 1,143.15 | -5.53 |
| Median decode, tok/s | 32.57 | 33.00 | +0.42 |
| Median prefill, ms | 28,526.470 | 28,664.568 | +138.098 |
| Median decode, ms | 15,687.324 | 15,486.910 | -200.414 |
| Peak process VRAM, MiB | 7,928 | 7,928 | 0 |
| Maximum process RSS high-water mark, KiB | 21,480,528 | 21,480,852 | +324 |
| Process swap, KiB / system swap-out growth, pages | 0 / 0 | 0 / 0 | 0 / 0 |

The first A-B-B-A control had 36,087 pages of system swap-out
and is excluded from timing acceptance. The first B is also
outside the balanced subset. The clean comparison uses
B2-A2-A3-B3. All six runs return identical text, 512 IDs,
probability rows and stop reason. Each arm reports 570,364
SLRU hits, 70,688 fills, 67,573 evictions and 135,674,634,240
uploaded expert bytes. The model is fully resident in RAM.
Q4_K and Q6_K eviction fixtures produce byte-identical output
with 48 fills, 40 evictions and zero Compute Sanitizer errors.

One separate A/B at the previous 5,920 MiB setting yields
1,132.22 / 33.19 and 1,141.39 / 34.12 tok/s for prefill / decode.
Decode falls from 15,398.401 to 14,976.249 ms. Both runs reach
8,218 MiB process VRAM, with RSS high-water marks of 21,480,076
and 21,479,648 KiB respectively. Process swap and system
swap-out growth stay at zero. All response fields and SLRU
counters match within this pair. This is a diagnostic pair.
The earlier 8,226 MiB EXP-1617 run also had a 5,920 MiB budget.
The 7,928 MiB result above uses a budget 288 MiB smaller.

### Expert transfers and GPU idle

The second pair uses Nsight Systems 2026.6.1 with node-level
and device event tracing. It covers all 511 decode evaluations.
Both traces have the same 998,476 kernel events and the same
graph-size distribution. Of the 40 per-layer regions per
step, 29 contain 49 kernels, 10 contain 48 kernels and the
final region contains 22 kernels. There are also 2,550
two-kernel replays. Host `cuLaunchKernel` calls include
kernel recording during capture.

| Decode measurement | Synchronous | Staged | Staged minus synchronous |
|---|---:|---:|---:|
| Server decode, ms | 18,642.954 | 18,180.012 | -462.941 |
| Device window, ms | 18,639.824 | 18,176.573 | -463.251 |
| GPU idle union, ms | 8,668.410 | 7,182.347 | -1,486.063 |
| Synchronous expert H2D calls | 62,571 | 0 | -62,571 |
| Asynchronous expert H2D calls | 0 | 62,571 | +62,571 |
| Expert H2D payload, bytes | 40,850,890,752 | 40,850,890,752 | 0 |
| Host elapsed in expert copy APIs, ms | 4,179.159 | 207.782 | -3,971.377 |
| GPU idle during expert copy APIs, ms | 2,793.955 | 162.694 | -2,631.260 |
| Expert H2D device time, ms | 1,709.259 | 2,744.811 | +1,035.552 |
| Expert H2D intersecting compute kernels, ms | 0.000 | 0.000 | 0.000 |
| Remaining synchronous H2D (8,192 B each) | 1,021 | 1,021 | 0 |
| Host elapsed in remaining synchronous H2D, ms | 18.459 | 18.045 | -0.414 |
| Ring-buffer host event synchronizations | 0 | 13,761 | +13,761 |
| Ring-buffer host event synchronization wall, ms | 0.000 | 13.110 | +13.110 |
| Copy-stream wait for old cache readers, ms | 0.000 | 6.666 | +6.666 |
| Graph-stream weight-ready event waits, ms | 0.000 | 25.661 | +25.661 |

The 2,682.964 ms of synchronous expert-copy idle identified
in EXP-1618 has no corresponding synchronous expert calls in
the new staged trace. Its fresh same-binary control measures
2,793.955 ms. The staged copy enqueue APIs still intersect
162.694 ms of GPU idle. Event timings and API intervals are
diagnostic subsets. GPU transfer duration, event waits and
host API duration must not be added to overall GPU idle.
Device event tracing can perturb scheduling, so performance
acceptance uses the unprofiled pair above.

All 62,571 staged copies associate with 10,974 graph replays.
Every miss group completes its last H2D before its first
compute kernel, including 10,970 groups ready before the
host calls `cuGraphLaunch`. The elapsed time between the last
copy and that call totals 1,274.413 ms, with 1,274.067 ms
of GPU idle. Within those intervals, host API time includes
384.621 ms recording kernels, 179.021 ms updating graph
executables and 22.578 ms in begin/end capture and
instantiation. Another 145.167 ms is async packet upload API
time. The device weight-ready waits are observed separately.
The span between the last independent kernel and first routed
matvec is 17.694 ms in miss groups, with zero time where a
required copy remains incomplete.

The staged transport removes blocking expert H2D from decode.
The new ordering provides independent compute nodes, but
per-token capture and update keep their replay behind the
completed transfers in this trace. Reusing a recorded region
with correct mutable addresses is the next measured target.
Host Top-K consumption and the remaining 8 KiB copies are
still present. Both graph and staging flags remain experimental.

Evidence is in
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/decode-region-exp1619`.
`profiles/analyze.py` joins CUDA API events, GPU transfers
and graph kernel replays. `profiles/expert-comparison.json`
contains the unrounded numbers. The response control hash
over IDs, text, probabilities and stop reason is
`d80bce6a8f64b63856c70215a2195a3402e1840a5d4888284b4f3e5a0d0460ba`.

## EXP-1618 baseline snapshot

EXP-1618 compares frozen [Optimized Hybrid](OPTIMIZED-HYBRID.md)
against independent Goose EXP-1617 (`efa4c82c`) with its cumulative
experimental graph, including the 18-kernel FFN. Both use RTX 4070,
the same Apex Q4_K_M weights and request, context 262,144, Q4_0 K/V,
4096/4096, CPU limits 8/16 with masks `0f0f` and `ffff`, and
a 5,632 MiB expert-cache budget. Each request evaluates 32,768
prompt IDs, reuses zero prompt tokens and returns 512 IDs.
The prompt and sampling controls are equal. The two runtimes
generate different continuations, so their expert routes can differ.
Each runtime's CUDA and CPU passes return the same complete
response and probability rows within that runtime.

| Profiled request | Optimized Hybrid | Goose EXP-1617 |
|---|---:|---:|
| CUDA-pass prefill, ms | 12,524.921 | 28,925.247 |
| CUDA-pass decode, ms | 8,639.381 | 17,574.224 |
| CUDA-pass prefill / decode, tok/s | 2616.22 / 59.15 | 1132.85 / 29.08 |
| CPU-pass prefill / decode, tok/s | 2665.32 / 64.35 | 1123.09 / 31.95 |
| CUDA-pass peak process VRAM, MiB | 8870 | 7966 |
| CPU-pass peak process VRAM, MiB | 8842 | 7936 |
| CUDA-pass peak RSS, KiB | 22517792 | 22076156 |
| CPU-pass peak RSS, KiB | 21876644 | 21490272 |
| Process swap and request system swap-out growth | 0 | 0 |

## Attribution and refresh

The CUDA pass uses NVIDIA Nsight Systems CLI 2026.6.1 with
node-level graph tracing. The full hybrid trace and the stopped
Goose trace include all 511 decode evaluations. A combined
collector run failed, and two Goose traces kept only 496 full
decode steps. They are excluded. Stop the named Nsight session
while the server is still alive to capture the end of the request.
The CPU passes use separate GNU gprofng clock sampling at 2 ms
with signals bracketing the request. CPU phase splits are
estimated from the request-start signal and each CPU pass's
reported prefill duration. These are sampled CPU milliseconds
across threads, separate from CUDA API wall time.
[Nsight documentation](https://docs.nvidia.com/nsight-systems/UserGuide/)
[GNU gprofng documentation](https://sourceware.org/binutils/docs/gprofng.html)

Device windows begin at the first 4,096-row RMSNorm and split at
the first one-token GDN execution after prefill. The hybrid request
trace has 90 earlier warm-up and tail-prefill GDN markers. The
window ends at the final output D2H or kernel, whichever is later.
Some host work before and after these landmarks sits outside the
device windows. Server timing rows keep that distinction visible.

GPU sum counts every clipped kernel or copy interval in its row.
Exclusive GPU time measures the interval during which only that
family is active. The busy union counts shared regions once. The
idle remainder is time without recorded GPU activity. The
exclusive differences locate measured exposure on the device
timeline. A speedup of any particular size still requires an
ablation because overlap and scheduling can change. CUDA API
overlap with GPU idle time identifies work to investigate, with
no causal attribution to the API itself. Graph-node rows are
subsets of kernel activity and never added to the GPU total.

Recompute the report after a change in the active graph or the
hybrid reference. Use the same settings, retain raw traces and
sampling data, record exact executable and request hashes and
qualify responses. Keep the previous snapshot and the unprofiled
acceptance measurements. Use the map to choose changes after
the Goose graph reaches the hybrid's next-Top-K boundary.

The [reproducible analyzer](../../tools/apex-latency-map.py)
checks the request bytes and step counts and produces JSON with
per-kernel and per-API rows. EXP-1618 evidence is under
`/home/end0/AI/LlamaServer/nsight-profile-exp1618.sNdw9P`.
It contains the reports, SQLite exports, CPU experiments, exact
commands, comparison JSON, and detailed tables. Accepted CUDA
report SHA-256 values are
`2a9ec4183759817ba32dacc52fa595f5cd1065464ed1793c26b639cc2585dac6`
for Optimized Hybrid and
`ec496f90713eb093840a9da008625c12c3dcdfcb485bcdc99b2ec210c8298ad2`
for Goose.

## Disjoint trace windows

| Phase / interval | Hybrid, ms | Goose, ms | Goose minus Hybrid, ms |
|---|---:|---:|---:|
| Prefill device window | 12,146.247 | 28,907.250 | +16,761.003 |
| Prefill busy GPU union | 11,036.994 | 25,407.219 | +14,370.226 |
| Prefill no recorded GPU activity | 1,109.254 | 3,500.030 | +2,390.777 |
| Prefill server time outside the device window | 378.674 | 17.997 | -360.676 |
| Decode device window | 8,632.618 | 17,571.051 | +8,938.433 |
| Decode busy GPU union | 6,610.551 | 10,113.189 | +3,502.637 |
| Decode no recorded GPU activity | 2,022.067 | 7,457.862 | +5,435.795 |
| Decode server time outside the device window | 6.763 | 3.173 | -3.590 |

## Ranked measured excess
Positive excess is ranked by exclusive GPU time or the idle
remainder. The ranking is a priority for investigation. It
does not predict the saving from changing any one operation.

| Phase | Family or gap | Hybrid exclusive / idle, ms | Goose exclusive / idle, ms | Excess, ms |
|---|---|---:|---:|---:|
| Prefill | MMQ | 3,142.227 | 11,638.307 | +8,496.080 |
| Decode | No recorded GPU work | 2,022.067 | 7,457.862 | +5,435.795 |
| Prefill | No recorded GPU work | 1,109.254 | 3,500.030 | +2,390.777 |
| Decode | Vector Attention | 1,221.339 | 2,838.630 | +1,617.290 |
| Prefill | Flash Attention MMA | 1,422.438 | 2,976.752 | +1,554.315 |
| Prefill | Pointwise and GLU | 491.680 | 1,648.458 | +1,156.778 |
| Prefill | GDN | 470.970 | 1,360.016 | +889.046 |
| Decode | Dense and generic matmul | 148.121 | 798.974 | +650.854 |
| Decode | Pointwise and GLU | 139.757 | 579.186 | +439.429 |
| Prefill | Dense and generic matmul | 86.341 | 499.674 | +413.333 |
| Decode | MMVQ | 2,485.502 | 2,815.432 | +329.930 |
| Prefill | H2D bulk | 3,497.644 | 3,775.733 | +278.089 |
| Decode | MoE router | 48.592 | 199.804 | +151.212 |
| Decode | RMSNorm | 124.993 | 273.368 | +148.375 |
| Prefill | Tensor gather and blit | 375.620 | 493.316 | +117.695 |
| Decode | H2D packets up to 4 KiB | 7.468 | 114.832 | +107.363 |
| Prefill | RMSNorm | 242.274 | 345.781 | +103.507 |
| Prefill | KV FWHT | 47.670 | 150.539 | +102.868 |
| Decode | GDN | 57.224 | 116.157 | +58.932 |
| Prefill | Other Attention | 0.000 | 58.055 | +58.055 |
| Decode | MMVQ pack | 184.144 | 233.103 | +48.959 |
| Prefill | D2D | 44.153 | 90.345 | +46.191 |
| Decode | KV FWHT | 18.108 | 54.504 | +36.396 |
| Decode | SSM convolution | 21.127 | 51.244 | +30.117 |
| Prefill | RoPE | 12.097 | 41.509 | +29.412 |
| Decode | KV writes | 14.544 | 34.848 | +20.304 |
| Decode | MoE weighted reduction | 29.555 | 46.858 | +17.302 |
| Decode | RoPE | 10.798 | 27.254 | +16.456 |
| Prefill | MoE router | 8.014 | 17.094 | +9.080 |
| Prefill | KV writes | 5.593 | 11.768 | +6.175 |
| Prefill | MoE weighted reduction | 242.257 | 243.543 | +1.286 |
| Prefill | Memset | 0.233 | 0.284 | +0.050 |

Goose overlaps about 1.475 s of additional prefill MMQ
with other GPU activity. The family sum grows by 9.971 s
while exclusive time grows by 8.496 s. Bulk H2D grows
by 1.753 s in the sum and by 0.278 s in exclusive time.
Use the disjoint window before attributing the full gap.

The decode CUDA API union overlaps 5.576 s of the 7.458 s
Goose idle remainder. Synchronous CUDA copy calls alone
overlap 3.665 s of that idle time. The analogous CUDA API
union overlaps 1.168 s of the 2.022 s hybrid remainder.
These observations include waits. A larger graph can remove
some handoffs. The map does not assign the entire idle delta
to graph replay or to the copy engine.

## GPU families and transfers

### Prefill

| Family | Hybrid count | Goose count | Hybrid sum, ms | Goose sum, ms | Δ sum, ms | Δ exclusive, ms |
|---|---:|---:|---:|---:|---:|---:|
| MMQ | 3,392 | 5,410 | 3,142.227 | 13,112.908 | +9,970.682 | +8,496.080 |
| Flash Attention MMA | 440 | 240 | 1,422.438 | 2,976.752 | +1,554.315 | +1,554.315 |
| Pointwise and GLU | 3,614 | 4,867 | 491.680 | 1,648.458 | +1,156.778 | +1,156.778 |
| GDN | 270 | 270 | 470.970 | 1,360.016 | +889.046 | +889.046 |
| Dense and generic matmul | 743 | 720 | 86.341 | 499.674 | +413.333 | +413.333 |
| H2D bulk | 58,075 | 158,212 | 3,497.644 | 5,250.378 | +1,752.734 | +278.089 |
| Tensor gather and blit | 1,216 | 632 | 375.620 | 493.316 | +117.695 | +117.695 |
| RMSNorm | 1,706 | 1,714 | 242.274 | 345.781 | +103.507 | +103.507 |
| KV FWHT | 360 | 360 | 47.670 | 150.539 | +102.868 | +102.868 |
| Other Attention | 0 | 20 | 0.000 | 58.055 | +58.055 | +58.055 |
| D2D | 15,894 | 320 | 44.153 | 90.345 | +46.191 | +46.191 |
| RoPE | 180 | 360 | 12.097 | 41.509 | +29.412 | +29.412 |
| MoE router | 352 | 1,080 | 8.014 | 17.094 | +9.080 | +9.080 |
| KV writes | 180 | 180 | 5.593 | 11.768 | +6.175 | +6.175 |
| MoE weighted reduction | 352 | 5,160 | 242.257 | 243.543 | +1.286 | +1.286 |
| Memset | 80 | 100 | 0.233 | 0.284 | +0.050 | +0.050 |
| MMVQ pack | 433 | 314 | 0.468 | 0.406 | -0.062 | -0.062 |
| SSM convolution | 271 | 271 | 203.979 | 203.799 | -0.180 | -0.180 |
| MMVQ | 433 | 314 | 8.311 | 8.050 | -0.261 | -0.261 |
| MMQ fixup | 480 | 0 | 3.134 | 0.000 | -3.134 | -3.134 |
| H2D packets up to 4 KiB | 6,685 | 4,030 | 4.868 | 1.729 | -3.139 | -3.151 |
| MMQ pack | 3,392 | 4,200 | 458.957 | 365.324 | -93.633 | -93.638 |
| D2H | 473 | 361 | 113.556 | 2.138 | -111.418 | -111.418 |
| MoE grouping | 936 | 0 | 154.508 | 0.000 | -154.508 | -154.508 |

### Decode

| Family | Hybrid count | Goose count | Hybrid sum, ms | Goose sum, ms | Δ sum, ms | Δ exclusive, ms |
|---|---:|---:|---:|---:|---:|---:|
| Vector Attention | 10,220 | 15,330 | 1,221.340 | 2,838.630 | +1,617.290 | +1,617.290 |
| Dense and generic matmul | 40,880 | 40,880 | 148.129 | 798.979 | +650.849 | +650.854 |
| Pointwise and GLU | 148,698 | 276,445 | 139.787 | 579.204 | +439.418 | +439.429 |
| MMVQ | 199,798 | 220,238 | 2,485.531 | 2,815.461 | +329.930 | +329.930 |
| MoE router | 20,440 | 61,320 | 48.592 | 199.810 | +151.218 | +151.212 |
| RMSNorm | 97,598 | 97,598 | 124.999 | 273.368 | +148.368 | +148.375 |
| H2D packets up to 4 KiB | 23,504 | 312,213 | 7.468 | 114.832 | +107.363 | +107.363 |
| GDN | 15,330 | 15,330 | 57.226 | 116.157 | +58.931 | +58.932 |
| MMVQ pack | 199,798 | 158,918 | 184.174 | 233.118 | +48.944 | +48.959 |
| KV FWHT | 20,440 | 20,440 | 18.111 | 54.504 | +36.393 | +36.396 |
| SSM convolution | 15,329 | 15,329 | 21.130 | 51.244 | +30.115 | +30.117 |
| KV writes | 10,220 | 10,220 | 14.544 | 34.848 | +20.304 | +20.304 |
| MoE weighted reduction | 20,440 | 20,440 | 29.556 | 46.860 | +17.304 | +17.302 |
| RoPE | 10,220 | 10,220 | 10.798 | 27.254 | +16.456 | +16.456 |
| D2H | 20,951 | 20,951 | 40.106 | 39.637 | -0.469 | -0.469 |
| H2D bulk | 67,626 | 120,311 | 1,850.626 | 1,806.046 | -44.580 | -44.580 |
| Tensor gather and blit | 87,888 | 35,768 | 208.506 | 83.284 | -125.222 | -125.206 |

## MMQ by recorded weight format
Format comes from the hybrid template or the specialized Goose
kernel name. The generic Goose MMQ path remains unresolved.
These are format-wide totals on the same Apex workload. A
launch grid is not a complete matrix shape, so these rows do
not isolate matched M/N/K problems or individual optimizations.

| Format | Hybrid calls | Goose calls | Hybrid sum, ms | Goose sum, ms | Δ sum, ms | Δ exclusive, ms |
|---|---:|---:|---:|---:|---:|---:|
| Q4_K | 2,928 | 4,050 | 2,387.148 | 9,718.617 | +7,331.470 | +5,856.868 |
| Q6_K | 464 | 1,020 | 755.079 | 2,413.051 | +1,657.972 | +1,657.972 |
| Unresolved format | 0 | 340 | 0.000 | 981.240 | +981.240 | +981.240 |

The archive's mmq-launch-geometry.tsv keeps exact template names,
grid and block dimensions, count and mean duration. The next
optimization pass needs matching logical tensor dimensions and
actual dispatch to turn this census into a per-shape comparison.

## CUDA Graph replay and host submission

Graph sizes count kernels sharing a replay correlation inside
the decode window. The results show the hybrid's dominant
43–44-kernel scope and the experimental Goose graph's 2-, 3-
and 18-kernel scopes. Zero in this table means no replay of
that size. Node durations are already in the family totals.

| Kernels per replay | Hybrid replays | Goose replays | Hybrid kernel sum, ms | Goose kernel sum, ms | Δ, ms |
|---:|---:|---:|---:|---:|---:|
| 2 | 0 | 97,411 | 0.000 | 1,714.379 | +1,714.379 |
| 3 | 0 | 5,110 | 0.000 | 2,838.630 | +2,838.630 |
| 18 | 0 | 20,400 | 0.000 | 2,049.064 | +2,049.064 |
| 20 | 510 | 0 | 487.006 | 0.000 | -487.006 |
| 27 | 510 | 0 | 48.352 | 0.000 | -48.352 |
| 43 | 4,581 | 0 | 1,699.184 | 0.000 | -1,699.184 |
| 44 | 14,763 | 0 | 2,268.157 | 0.000 | -2,268.157 |
| 46 | 509 | 0 | 192.333 | 0.000 | -192.333 |

Both timelines contain more than half a million nodes produced
by graph launches. Goose nevertheless submits 422,229 direct
CUDA calls in decode, versus 6,736 in Optimized Hybrid. The
dominant Goose 18-node group replays 20,400 times. The remaining
target is the chain through the following layer's recurrent or
attention work and next host-visible Top-K.

### Prefill CUDA APIs

| API family | Hybrid calls | Goose calls | Hybrid wall sum, ms | Goose wall sum, ms | Δ wall, ms | Hybrid / Goose overlap with GPU idle, ms |
|---|---:|---:|---:|---:|---:|---:|
| All CUDA APIs, union | 103,206 | 220,716 | 11,495.934 | 22,926.822 | +11,430.887 | 596.548 / 2,203.205 |
| Asynchronous memcpy API | 81,127 | 103,672 | 414.206 | 314.489 | -99.717 | 81.670 / 170.628 |
| CUDA allocation and mapping | 84 | 151 | 0.135 | 22.810 | +22.675 | 0.121 / 8.233 |
| CUDA synchronization and waits | 1,728 | 15,831 | 10,788.984 | 2,974.623 | -7,814.360 | 297.601 / 8.221 |
| Direct launch | 18,751 | 26,112 | 83.922 | 107.471 | +23.549 | 11.286 / 43.654 |
| Other CUDA API | 1,516 | 15,699 | 208.688 | 33.759 | -174.930 | 205.870 / 18.983 |
| Synchronous memcpy API | 0 | 59,251 | 0.000 | 19,473.670 | +19,473.670 | 0.000 / 1,953.487 |

### Decode CUDA APIs

| API family | Hybrid calls | Goose calls | Hybrid wall sum, ms | Goose wall sum, ms | Δ wall, ms | Hybrid / Goose overlap with GPU idle, ms |
|---|---:|---:|---:|---:|---:|---:|
| All CUDA APIs, union | 199,771 | 1,040,944 | 7,677.909 | 15,036.544 | +7,358.635 | 1,167.945 / 5,575.641 |
| Asynchronous memcpy API | 112,081 | 312,213 | 425.953 | 910.937 | +484.985 | 232.821 / 224.582 |
| CUDA allocation and mapping | 0 | 1 | 0.000 | 0.015 | +0.015 | 0.000 / 0.015 |
| CUDA synchronization and waits | 59,766 | 26,060 | 5,963.481 | 1,105.418 | -4,858.062 | 395.938 / 220.557 |
| Direct launch | 6,736 | 422,229 | 16.568 | 1,269.278 | +1,252.709 | 1.851 / 480.956 |
| Graph launch | 20,873 | 122,921 | 1,256.972 | 1,361.493 | +104.522 | 529.579 / 947.275 |
| Graph setup and update | 314 | 16,257 | 14.038 | 38.791 | +24.753 | 6.859 / 37.109 |
| Other CUDA API | 1 | 1 | 0.898 | 0.012 | -0.886 | 0.898 / 0.012 |
| Synchronous memcpy API | 0 | 141,262 | 0.000 | 10,350.600 | +10,350.600 | 0.000 / 3,665.137 |

No target-process OS wait rows matched the named wait filters
inside these device windows. This leaves unobserved scheduling
and waiting open. API wall sums can include earlier GPU work and
must remain separate from sampled CPU time and exclusive GPU time.

## Decode idle attribution for EXP-1618

The `--decode-idle` analyzer option partitions the same device idle
windows. It joins host CUDA calls to kernels and copies by process
and CUPTI correlation ID. Host APIs in each decode window come from
one thread. There are zero recorded CUDA API call chains, zero
target-process OS runtime rows, and no CPU scheduling trace. The
separate clock profiles cannot identify which functions ran inside
specific idle intervals.

| Disjoint idle observation | Hybrid calls | Goose calls | Hybrid, ms | Goose, ms | Δ, ms | Module or next test |
|---|---:|---:|---:|---:|---:|---|
| Synchronous expert H2D | 0 | 62,571 | 0.000 | 2,682.964 | +2,682.964 | Expert cache plane fills. Test staged transfers with explicit slot and consumer dependencies. |
| No CUDA API, next submit has not started | — | — | 825.915 | 1,366.265 | +540.350 | Submission boundary is measured. Time spent in individual host functions is unresolved. |
| No CUDA API, next activity already submitted | — | — | 28.207 | 515.956 | +487.749 | Next device activity and its completed submit are correlated. Device or driver wait reason is unresolved. |
| Direct kernel launch | 6,736 | 422,229 | 1.851 | 480.956 | +479.104 | Driver launch calls in the graph executor. Compare the extended graph. |
| Graph launch | 20,873 | 122,921 | 529.579 | 947.275 | +417.696 | Driver replay calls. Separate gaps before their first node from gaps between nodes. |
| Synchronous H2D, 8,192 B | 0 | 42,411 | 0.000 | 378.349 | +378.349 | Mostly adjacent to RMSNorm. Verify the staged tensor, then reuse device storage or retain an ordered pinned copy. |
| Top-K IDs, synchronous D2H | 0 | 20,440 | 0.000 | 349.643 | +349.643 | Host SLRU consumes expert IDs. Preserve readiness while changing how the IDs reach the host. |
| SSM convolution input, synchronous H2D | 0 | 15,329 | 0.000 | 211.689 | +211.689 | `cuda_position` and `fg_cuda_position`. Confirm the operand address and test GPU residency. |
| Logits, synchronous D2H | 0 | 511 | 0.000 | 42.492 | +42.492 | Graph output publication to host sampling. A GPU sampler would need to preserve the response and probabilities contract. |
| Graph setup and node update | 314 | 16,257 | 6.859 | 37.109 | +30.250 | Graph instantiation and parameter updates. Test stable addresses and replay reuse. |
| CUDA allocation and mapping | 0 | 1 | 0.000 | 0.015 | +0.015 | Allocation within the measured window. |
| Other CUDA API | 1 | 1 | 0.898 | 0.012 | -0.886 | Remaining API events. |
| Asynchronous copy API | 112,081 | 312,213 | 232.821 | 224.582 | -8.239 | Enqueue calls on both runtimes. |
| CUDA synchronization and waits | 59,766 | 26,060 | 395.938 | 220.557 | -175.381 | Hybrid stream waits and Goose context waits. This row counts only their GPU-idle overlap. |
| **Total** | **199,771** | **1,040,944** | **2,022.067** | **7,457.862** | **+5,435.795** | Every idle nanosecond appears once. |

Rows give exact temporal attribution. The intervals are an
exposure measurement, with no assumption that removing a call
would recover the whole interval. The analyzer also reports
the number of distinct idle intervals each row touches. There
were no overlapping target CUDA API calls in either decode
window. Displayed milliseconds are rounded independently.

### Handoffs outside CUDA and graph launch

| Timing evidence | Hybrid, ms | Goose, ms | What is established |
|---|---:|---:|---|
| No active CUDA API before next submit | 825.915 | 1,366.265 | 55,956 Hybrid and 72,361 Goose idle intervals touch this row. |
| Logits readback returned, next 8,192 B upload has not started | — | 1,232.479 | Inter-token host window, a subset of the preceding row. |
| Top-K readback returned, next expert H2D has not started | — | 30.545 | Brackets the host route and SLRU path, a subset of the first row. |
| No active CUDA API and next activity already submitted | 28.207 | 515.956 | 42,191 Hybrid and 629,122 Goose idle intervals touch this row. |
| Same graph replay on both sides of an uncovered gap | 0.007 | 1.563 | Subset of the already-submitted row between activities of the same replay. |
| During graph launch before first correlated activity | 529.578 | 947.209 | Most graph-launch idle overlap is before the next replay starts. |
| During graph launch between correlated activities | 0.001 | 0.009 | Idle inside the replay's recorded activity span. |
| During graph launch after last correlated activity | 0.000 | 0.057 | Idle after the replay's last recorded activity while the launch call is still active. |

The dominant untraced Goose boundary starts after publishing
logits for CPU sampling. `q35_sample_generate` selects the
token and formats requested probabilities before the next
`q35_generation_eval`. `q35_model_step` constructs the next
graph and opens the KV ticket before device execution. These
are source-confirmed steps inside the measured handoff.
Their individual wall durations, allocator contribution and
time off CPU remain unknown. The Hybrid's host-only time
before an asynchronous copy totals 810.899 ms. The 1,232.479 ms
Goose handoff is already included in the +540.350 ms comparison.

The Top-K to first expert upload boundary contains CPU route
extraction, SLRU planning and upload preparation. Its 30.545 ms
measures the interval between CUDA APIs. Another 41.146 ms
after Top-K precedes an asynchronous packet upload. Those
intervals are separate from blocking expert uploads.

The 515.956 ms with a completed submit before the next Goose
device activity includes 180.626 ms before direct kernel
activity, 169.356 ms before graph activity, 124.898 ms before
async copies and 41.076 ms before synchronous API copies.
This establishes submission order. Driver queueing, stream
dependencies and hardware scheduling remain unresolved
within these intervals.

### Synchronous copies

All 141,262 blocking API calls match one recorded GPU transfer
on stream 7. GPU copy time is shown separately and is already
in the GPU busy union. "Before" and "after" refer to the linked
copy's timestamps and count only time inside the API call with
no recorded GPU activity.

| Direction and host memory | Bytes per call | Calls | API wall, ms | GPU copy, ms | Idle before copy, ms | Idle after copy, ms |
|---|---:|---:|---:|---:|---:|---:|
| Expert H2D, pageable | 589,824 | 34,097 | 2,140.691 | 818.338 | 1,629.647 | 0.000 |
| Expert H2D, pageable | 860,160 | 14,593 | 1,200.172 | 503.909 | 999.660 | 0.000 |
| Expert H2D, pinned | 589,824 | 13,881 | 706.844 | 362.129 | 26.573 | 27.084 |
| Activation or norm input H2D, pageable | 8,192 | 42,411 | 5,401.968 | 23.627 | 377.754 | 0.595 |
| SSM convolution input H2D, pageable | 131,072 | 15,329 | 212.079 | 98.043 | 211.668 | 0.021 |
| Top-K IDs D2H, pageable | 32 | 20,440 | 626.070 | 19.352 | 298.366 | 51.276 |
| Logits D2H, pageable | 993,280 | 511 | 62.777 | 20.285 | 3.043 | 39.450 |

In total, 3,546.712 ms of the copy API idle overlap precedes
the associated transfer and 118.425 ms follows it. The 1,845.684
ms of actual copy-engine activity is separate. Of 106,430
pageable H2D calls, 104,162 return before the associated
device copy starts. No pinned expert H2D or synchronous
D2H call in this trace returns before its device copy starts.

The source below refers to the profiled `efa4c82c` build.

- **Expert fills, high confidence.** All 62,571 copies fall between
  `goose_route_topk` and `goose_mmvq_quantize`. Each cache fill
  uploads three planes, with 47,978 Q4_K transfers and 14,593
  Q6_K transfers in the window. `fg_cuda_routed_graph` reads the
  IDs, resolves active routes and calls `fg_cuda_prepare_experts`.
  `ec_prepare` calls `slru_plan_impl` and sends each plane via
  `ec_fill_plane → fg_cuda_expert_upload → expert_upload`.
  `fg_cuda_expert_stage_enabled` rejects eight or fewer active
  experts, so the decode path remains synchronous even when
  `GOOSE_CUDA_EXPERT_OVERLAP=1`. Slot reuse must follow the
  previous expert read, and new projections must wait for their
  own weights. Those are device ordering constraints. The
  current per-plane host blocking is an implementation choice.
  Source [SLRU and plane fills](../../goose/model/expert-device-cache.goose)
  and [CUDA cache transport](../../integration/goose-standalone/cuda-driver.c).
- **Top-K IDs, high confidence.** All 20,440 transfers sit
  between the Top-K and the routed input quantizer, one
  eight-element i32 vector for every layer and decode step.
  `fg_cuda_routed_graph → fg_cuda_activation_read_address →
  activation_download_record` supplies IDs to the CPU SLRU.
  This consumer must see completed Top-K results before it
  selects slots. A pinned async copy alone still needs a
  host-visible completion point.
  Source [CUDA routed graph](../../integration/goose-standalone/cuda-driver.c).
- **8,192-byte H2D, strong association with vector inputs,
  unresolved individual operands.** The next kernel is
  `goose_rms_reference` for 41,389 calls and
  `goose_vector_ops` for 1,022. 40,369 are between
  `goose_vector_ops` and `goose_rms_reference`.
  This size equals one 2,048-element F32 vector. The
  corresponding source path is `fg_cuda_vector → vector_input →
  activation_input → upload_packet`, or the compact input
  branch in `vector_input`. The trace has no source pointer
  or stack to split norm weights, activations and compact
  inputs. `upload_packet` uses synchronous H2D above 4,096 B.
  A device-only consumer requires ordered device access and
  stable staging storage. Capture both operand identities
  before making a residency change.
  Source [vector input staging](../../integration/goose-standalone/cuda-driver.c).
- **SSM input, high confidence on the operation.** All 15,329
  transfers sit between `goose_tensor_blit` and
  `goose_ssm_conv`. The call path is
  `cuda_position → fg_cuda_position → upload_inputs →
  activation_input → upload_packet`. The 131,072-byte input
  is consistent with the recurrent convolution's F32 operand
  shape. The trace cannot distinguish the exact host operand
  address. GPU residency or pinned staging must preserve the
  current recurrent-state ordering.
  Source [SSM dispatch](../../goose/cpu/goose-cpu-backend-api/cuda-position.goose)
  and [CUDA input staging](../../integration/goose-standalone/cuda-driver.c).
- **Logits, high confidence.** There is one 248,320-element
  F32 readback following the final matvec on each step.
  `fg_cuda_activation_graph_end → fg_cuda_activation_read_address →
  activation_download_record` publishes the output for
  host sampling. The current sampler requires completed logits.
  Source [graph output](../../integration/goose-standalone/graph-execute.c)
  and [step generation](../../goose/model/qwen35-generation.goose).

The CPU-only profile has 378 ms of sampled execution for Goose
and 200 ms for Hybrid in decode, with 228 and 120 ms respectively
in the CUDA driver. Goose samples include `fg_cuda_vector`
(4 ms exclusive, 142 ms inclusive), `fg_cuda_routed_graph`
(2 ms exclusive, 96 ms inclusive) and `execute_graph_node_g`
(2 ms exclusive, 196 ms inclusive). These inclusive stacks
overlap. They do not measure the wall duration of
`ec_prepare`, SLRU, dispatch or off-CPU waits. Do not add
them to the idle rows.

The archived machine-readable expansion is
`decode-idle-exp1618.json` in the EXP-1618 evidence directory.
Its SHA-256 is
`e1e56d72f6db3dc083222a28041108aa28578849af9c929d117e610f431d7270`.
It retains the seven transfer fingerprints, nearest kernels,
API counts, unlabeled intervals by their neighboring calls,
and integer nanoseconds for reconciliation.

## Sampled CPU execution

Clock samples assign exclusive time to the leaf function. Counts
are exclusive CPU milliseconds divided by the 2 ms interval.
Inclusive stack context is retained in the raw report. CPU
phases are estimated on separate runs and do not establish
the causal owner of the idle GPU gaps.

### Prefill

| CPU category | Hybrid samples | Goose samples | Hybrid exclusive CPU, ms | Goose exclusive CPU, ms | Δ CPU, ms |
|---|---:|---:|---:|---:|---:|
| Total | 138 | 463 | 276 | 926 | +650 |
| System libraries | 31 | 175 | 62 | 350 | +288 |
| CUDA driver | 94 | 189 | 188 | 378 | +190 |
| CPU gather | 0 | 80 | 0 | 160 | +160 |
| Other CPU functions | 9 | 19 | 18 | 38 | +20 |
| Graph construction and dispatch | 4 | 0 | 8 | 0 | -8 |

### Decode

| CPU category | Hybrid samples | Goose samples | Hybrid exclusive CPU, ms | Goose exclusive CPU, ms | Δ CPU, ms |
|---|---:|---:|---:|---:|---:|
| Total | 100 | 189 | 200 | 378 | +178 |
| CUDA driver | 60 | 114 | 120 | 228 | +108 |
| System libraries | 24 | 45 | 48 | 90 | +42 |
| Other CPU functions | 11 | 27 | 22 | 54 | +32 |
| Graph construction and dispatch | 1 | 2 | 2 | 4 | +2 |
| Sampling and token selection | 4 | 1 | 8 | 2 | -6 |

## Exact recorded CUDA kernel and transfer names

An empty side has zero events with that exact spelling. The
GPU family table compares equivalent operations across names.
The per-symbol delta is an inventory difference. The exclusive
delta counts only time with that symbol active alone.

### Prefill

| CUDA kernel or transfer | Hybrid count | Goose count | Hybrid sum, ms | Goose sum, ms | Δ sum, ms | Δ exclusive, ms | Hybrid mean, µs | Goose mean, µs |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `H2D bulk` | 58,075 | 158,212 | 3,497.644 | 5,250.378 | +1,752.734 | +278.089 | 60.226 | 33.186 |
| `goose_mmq_q4_dense_x128` | 0 | 2,000 | 0.000 | 5,309.750 | +5,309.750 | +5,309.750 | 0.000 | 2,654.875 |
| `goose_mmq_q4_routed_x128` | 0 | 640 | 0.000 | 3,546.564 | +3,546.564 | +2,357.982 | 0.000 | 5,541.506 |
| `mul_mat_q` | 3,392 | 0 | 3,142.227 | 0.000 | -3,142.227 | -3,142.227 | 926.364 | 0.000 |
| `goose_flash_causal_mma` | 0 | 80 | 0.000 | 2,966.034 | +2,966.034 | +2,966.034 | 0.000 | 37,075.425 |
| `goose_vector_ops` | 0 | 4,867 | 0.000 | 1,648.458 | +1,648.458 | +1,648.458 | 0.000 | 338.701 |
| `flash_attn_ext_f16` | 90 | 0 | 1,402.655 | 0.000 | -1,402.655 | -1,402.655 | 15,585.056 | 0.000 |
| `goose_gdn_mma` | 0 | 240 | 0.000 | 1,359.241 | +1,359.241 | +1,359.241 | 0.000 | 5,663.503 |
| `goose_mmq_q6_dense_x128` | 0 | 320 | 0.000 | 1,248.969 | +1,248.969 | +1,248.969 | 0.000 | 3,903.028 |
| `goose_mmq_matmul` | 0 | 340 | 0.000 | 981.240 | +981.240 | +981.240 | 0.000 | 2,886.000 |
| `goose_mmq_q6_routed_x128` | 0 | 320 | 0.000 | 927.675 | +927.675 | +927.675 | 0.000 | 2,898.986 |
| `goose_mmq_q4_routed_x64` | 0 | 720 | 0.000 | 769.678 | +769.678 | +483.659 | 0.000 | 1,068.997 |
| `goose_tensor_blit` | 0 | 632 | 0.000 | 493.316 | +493.316 | +493.316 | 0.000 | 780.563 |
| `gdn_single` | 240 | 0 | 470.785 | 0.000 | -470.785 | -470.785 | 1,961.605 | 0.000 |
| `quantize_mmq_q8_1` | 3,392 | 0 | 458.957 | 0.000 | -458.957 | -458.957 | 135.306 | 0.000 |
| `goose_dense_mma_reference` | 0 | 280 | 0.000 | 407.036 | +407.036 | +407.036 | 0.000 | 1,453.698 |
| `goose_rms_reference` | 0 | 1,714 | 0.000 | 345.781 | +345.781 | +345.781 | 0.000 | 201.739 |
| `concat_non_cont` | 270 | 0 | 339.794 | 0.000 | -339.794 | -339.794 | 1,258.498 | 0.000 |
| `goose_mmq_pack_q8` | 0 | 3,480 | 0.000 | 311.752 | +311.752 | +311.752 | 0.000 | 89.584 |
| `unary_gated_op_kernel` | 1,064 | 0 | 296.873 | 0.000 | -296.873 | -296.873 | 279.016 | 0.000 |
| `goose_moe_weighted` | 0 | 5,160 | 0.000 | 243.543 | +243.543 | +243.543 | 0.000 | 47.198 |
| `rms_norm_f32` | 1,706 | 0 | 242.274 | 0.000 | -242.274 | -242.274 | 142.013 | 0.000 |
| `moe_weighted_reduction_f32` | 352 | 0 | 242.257 | 0.000 | -242.257 | -242.257 | 688.231 | 0.000 |
| `goose_mmq_q6_routed_x64` | 0 | 340 | 0.000 | 224.291 | +224.291 | +224.291 | 0.000 | 659.681 |
| `ssm_conv_long_token_f32` | 240 | 0 | 203.931 | 0.000 | -203.931 | -203.931 | 849.714 | 0.000 |
| `goose_ssm_conv_long` | 0 | 240 | 0.000 | 203.645 | +203.645 | +203.645 | 0.000 | 848.522 |
| `k_bin_bcast` | 1,597 | 0 | 193.310 | 0.000 | -193.310 | -193.310 | 121.046 | 0.000 |
| `mm_ids_helper` | 936 | 0 | 154.508 | 0.000 | -154.508 | -154.508 | 165.073 | 0.000 |
| `goose_kv_fwht` | 0 | 360 | 0.000 | 150.539 | +150.539 | +150.539 | 0.000 | 418.163 |
| `D2D` | 15,894 | 320 | 44.153 | 90.345 | +46.191 | +46.191 | 2.778 | 282.327 |
| `D2H` | 473 | 361 | 113.556 | 2.138 | -111.418 | -111.418 | 240.077 | 5.922 |
| `goose_mmq_q4_reference` | 0 | 690 | 0.000 | 92.625 | +92.625 | +92.625 | 0.000 | 134.240 |
| `Kernel2` | 312 | 0 | 59.211 | 0.000 | -59.211 | -59.211 | 189.780 | 0.000 |
| `goose_q4_attention_tiles` | 0 | 10 | 0.000 | 56.578 | +56.578 | +56.578 | 0.000 | 5,657.838 |
| `goose_dense_mma` | 0 | 40 | 0.000 | 55.244 | +55.244 | +55.244 | 0.000 | 1,381.091 |
| `goose_mmq_pack_shared` | 0 | 720 | 0.000 | 53.571 | +53.571 | +53.567 | 0.000 | 74.405 |
| `fwht_cuda` | 360 | 0 | 47.670 | 0.000 | -47.670 | -47.670 | 132.418 | 0.000 |
| `goose_quantized_matmul` | 0 | 400 | 0.000 | 37.395 | +37.395 | +37.395 | 0.000 | 93.487 |
| `cpy_scalar` | 361 | 0 | 32.937 | 0.000 | -32.937 | -32.937 | 91.240 | 0.000 |
| `goose_rope_apply` | 0 | 180 | 0.000 | 29.356 | +29.356 | +29.356 | 0.000 | 163.087 |
| `mul_mat_vec_f` | 314 | 0 | 26.643 | 0.000 | -26.643 | -26.643 | 84.850 | 0.000 |
| `dequantize_block_q4_0` | 180 | 0 | 12.878 | 0.000 | -12.878 | -12.878 | 71.543 | 0.000 |
| `goose_rope_phases` | 0 | 180 | 0.000 | 12.153 | +12.153 | +12.153 | 0.000 | 67.518 |
| `goose_mmq_q6_reference` | 0 | 40 | 0.000 | 12.115 | +12.115 | +12.115 | 0.000 | 302.878 |
| `rope_multi` | 180 | 0 | 12.097 | 0.000 | -12.097 | -12.097 | 67.207 | 0.000 |
| `goose_set_q4_rows` | 0 | 180 | 0.000 | 11.768 | +11.768 | +11.768 | 0.000 | 65.376 |
| `goose_flash_q4_stage` | 0 | 160 | 0.000 | 10.718 | +10.718 | +10.718 | 0.000 | 66.988 |
| `goose_route_reduce` | 0 | 360 | 0.000 | 8.488 | +8.488 | +8.488 | 0.000 | 23.577 |
| `goose_mmvq_matvec` | 0 | 314 | 0.000 | 8.050 | +8.050 | +8.050 | 0.000 | 25.637 |
| `topk_moe_cuda` | 352 | 0 | 8.014 | 0.000 | -8.014 | -8.014 | 22.766 | 0.000 |
| `H2D packets up to 4 KiB` | 6,685 | 4,030 | 4.868 | 1.729 | -3.139 | -3.151 | 0.728 | 0.429 |
| `k_set_rows_quant` | 180 | 0 | 5.593 | 0.000 | -5.593 | -5.593 | 31.073 | 0.000 |
| `goose_route_topk` | 0 | 360 | 0.000 | 5.149 | +5.149 | +5.149 | 0.000 | 14.302 |
| `mul_mat_vec_q_moe` | 117 | 0 | 4.669 | 0.000 | -4.669 | -4.669 | 39.910 | 0.000 |
| `flash_attn_mask_to_KV_max` | 80 | 0 | 4.293 | 0.000 | -4.293 | -4.293 | 53.661 | 0.000 |
| `mul_mat_vec_q` | 316 | 0 | 3.641 | 0.000 | -3.641 | -3.641 | 11.523 | 0.000 |
| `goose_route_norm` | 0 | 360 | 0.000 | 3.457 | +3.457 | +3.457 | 0.000 | 9.603 |
| `mul_mat_q_stream_k_fixup` | 480 | 0 | 3.134 | 0.000 | -3.134 | -3.134 | 6.530 | 0.000 |
| `flash_attn_stream_k_fixup_general` | 80 | 0 | 2.571 | 0.000 | -2.571 | -2.571 | 32.134 | 0.000 |
| `k_get_rows_float_vec` | 271 | 0 | 2.003 | 0.000 | -2.003 | -2.003 | 7.392 | 0.000 |
| `goose_q4_attention_merge` | 0 | 10 | 0.000 | 1.477 | +1.477 | +1.477 | 0.000 | 147.653 |
| `unary_op_kernel` | 893 | 0 | 1.175 | 0.000 | -1.175 | -1.175 | 1.316 | 0.000 |
| `k_get_rows_float` | 313 | 0 | 0.884 | 0.000 | -0.884 | -0.884 | 2.824 | 0.000 |
| `goose_gated_delta_net` | 0 | 30 | 0.000 | 0.775 | +0.775 | +0.775 | 0.000 | 25.830 |
| `Memset` | 80 | 100 | 0.233 | 0.284 | +0.050 | +0.050 | 2.917 | 2.837 |
| `quantize_q8_1` | 433 | 0 | 0.468 | 0.000 | -0.468 | -0.468 | 1.080 | 0.000 |
| `goose_mmvq_quantize` | 0 | 314 | 0.000 | 0.406 | +0.406 | +0.406 | 0.000 | 1.293 |
| `mul_mat_f` | 39 | 0 | 0.386 | 0.000 | -0.386 | -0.386 | 9.894 | 0.000 |
| `scale_f32` | 60 | 0 | 0.322 | 0.000 | -0.322 | -0.322 | 5.360 | 0.000 |
| `gated_delta_net_cuda` | 30 | 0 | 0.184 | 0.000 | -0.184 | -0.184 | 6.147 | 0.000 |
| `goose_ssm_conv` | 0 | 31 | 0.000 | 0.154 | +0.154 | +0.154 | 0.000 | 4.957 |
| `dot_kernel` | 39 | 0 | 0.053 | 0.000 | -0.053 | -0.053 | 1.370 | 0.000 |
| `ssm_conv_f32` | 31 | 0 | 0.048 | 0.000 | -0.048 | -0.048 | 1.541 | 0.000 |
| `reduce_1Block_kernel` | 39 | 0 | 0.047 | 0.000 | -0.047 | -0.047 | 1.214 | 0.000 |
| `flash_attn_stream_k_fixup_uniform` | 10 | 0 | 0.041 | 0.000 | -0.041 | -0.041 | 4.131 | 0.000 |
| `concat_cont` | 1 | 0 | 0.001 | 0.000 | -0.001 | -0.001 | 1.408 | 0.000 |

### Decode

| CUDA kernel or transfer | Hybrid count | Goose count | Hybrid sum, ms | Goose sum, ms | Δ sum, ms | Δ exclusive, ms | Hybrid mean, µs | Goose mean, µs |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `H2D bulk` | 67,626 | 120,311 | 1,850.626 | 1,806.046 | -44.580 | -44.580 | 27.366 | 15.011 |
| `goose_mmvq_matvec` | 0 | 220,238 | 0.000 | 2,815.461 | +2,815.461 | +2,815.432 | 0.000 | 12.784 |
| `goose_flash_vec_q4_packed` | 0 | 5,110 | 0.000 | 2,779.986 | +2,779.986 | +2,779.986 | 0.000 | 544.029 |
| `mul_mat_vec_q` | 199,798 | 0 | 2,485.531 | 0.000 | -2,485.531 | -2,485.502 | 12.440 | 0.000 |
| `flash_attn_ext_vec` | 5,110 | 0 | 1,212.987 | 0.000 | -1,212.987 | -1,212.987 | 237.375 | 0.000 |
| `goose_quantized_matmul` | 0 | 40,880 | 0.000 | 798.979 | +798.979 | +798.974 | 0.000 | 19.544 |
| `goose_vector_ops` | 0 | 276,445 | 0.000 | 579.204 | +579.204 | +579.186 | 0.000 | 2.095 |
| `goose_rms_reference` | 0 | 97,598 | 0.000 | 273.368 | +273.368 | +273.368 | 0.000 | 2.801 |
| `goose_mmvq_quantize` | 0 | 158,918 | 0.000 | 233.118 | +233.118 | +233.103 | 0.000 | 1.467 |
| `quantize_q8_1` | 199,798 | 0 | 184.174 | 0.000 | -184.174 | -184.144 | 0.922 | 0.000 |
| `mul_mat_vec_f` | 40,880 | 0 | 148.129 | 0.000 | -148.129 | -148.121 | 3.624 | 0.000 |
| `rms_norm_f32` | 97,598 | 0 | 124.999 | 0.000 | -124.999 | -124.993 | 1.281 | 0.000 |
| `H2D packets up to 4 KiB` | 23,504 | 312,213 | 7.468 | 114.832 | +107.363 | +107.363 | 0.318 | 0.368 |
| `goose_gdn_decode` | 0 | 15,330 | 0.000 | 116.157 | +116.157 | +116.157 | 0.000 | 7.577 |
| `k_get_rows_float_vec` | 15,329 | 0 | 99.514 | 0.000 | -99.514 | -99.511 | 6.492 | 0.000 |
| `goose_route_reduce` | 0 | 20,440 | 0.000 | 88.636 | +88.636 | +88.636 | 0.000 | 4.336 |
| `goose_tensor_blit` | 0 | 35,768 | 0.000 | 83.284 | +83.284 | +83.284 | 0.000 | 2.328 |
| `goose_route_topk` | 0 | 20,440 | 0.000 | 81.158 | +81.158 | +81.158 | 0.000 | 3.971 |
| `D2H` | 20,951 | 20,951 | 40.106 | 39.637 | -0.469 | -0.469 | 1.914 | 1.892 |
| `k_bin_bcast` | 56,720 | 0 | 61.664 | 0.000 | -61.664 | -61.655 | 1.087 | 0.000 |
| `k_get_rows_float` | 36,791 | 0 | 57.277 | 0.000 | -57.277 | -57.275 | 1.557 | 0.000 |
| `gated_delta_net_cuda` | 15,330 | 0 | 57.226 | 0.000 | -57.226 | -57.224 | 3.733 | 0.000 |
| `goose_kv_fwht` | 0 | 20,440 | 0.000 | 54.504 | +54.504 | +54.504 | 0.000 | 2.667 |
| `goose_ssm_conv` | 0 | 15,329 | 0.000 | 51.244 | +51.244 | +51.244 | 0.000 | 3.343 |
| `unary_gated_op_kernel` | 56,209 | 0 | 51.017 | 0.000 | -51.017 | -51.004 | 0.908 | 0.000 |
| `goose_flash_vec_combine` | 0 | 5,110 | 0.000 | 50.277 | +50.277 | +50.277 | 0.000 | 9.839 |
| `topk_moe_cuda` | 20,440 | 0 | 48.592 | 0.000 | -48.592 | -48.592 | 2.377 | 0.000 |
| `goose_moe_weighted` | 0 | 20,440 | 0.000 | 46.860 | +46.860 | +46.858 | 0.000 | 2.293 |
| `goose_set_q4_rows` | 0 | 10,220 | 0.000 | 34.848 | +34.848 | +34.848 | 0.000 | 3.410 |
| `cpy_scalar` | 20,439 | 0 | 31.178 | 0.000 | -31.178 | -31.168 | 1.525 | 0.000 |
| `goose_route_norm` | 0 | 20,440 | 0.000 | 30.016 | +30.016 | +30.010 | 0.000 | 1.469 |
| `moe_weighted_reduction_f32` | 20,440 | 0 | 29.556 | 0.000 | -29.556 | -29.555 | 1.446 | 0.000 |
| `goose_rope_decode` | 0 | 10,220 | 0.000 | 27.254 | +27.254 | +27.254 | 0.000 | 2.667 |
| `unary_op_kernel` | 35,769 | 0 | 27.105 | 0.000 | -27.105 | -27.092 | 0.758 | 0.000 |
| `ssm_conv_f32` | 15,329 | 0 | 21.130 | 0.000 | -21.130 | -21.127 | 1.378 | 0.000 |
| `concat_cont` | 15,329 | 0 | 20.537 | 0.000 | -20.537 | -20.528 | 1.340 | 0.000 |
| `fwht_cuda` | 20,440 | 0 | 18.111 | 0.000 | -18.111 | -18.108 | 0.886 | 0.000 |
| `k_set_rows_quant` | 10,220 | 0 | 14.544 | 0.000 | -14.544 | -14.544 | 1.423 | 0.000 |
| `rope_multi` | 10,220 | 0 | 10.798 | 0.000 | -10.798 | -10.798 | 1.057 | 0.000 |
| `goose_flash_vec_pack_query` | 0 | 5,110 | 0.000 | 8.367 | +8.367 | +8.367 | 0.000 | 1.637 |
| `flash_attn_combine_results` | 5,110 | 0 | 8.353 | 0.000 | -8.353 | -8.352 | 1.635 | 0.000 |
