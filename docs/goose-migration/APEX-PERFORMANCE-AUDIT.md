# Apex runtime performance and historical optimization audit

The [current Apex latency map](APEX-LATENCY-MAP.md) compares
the active Optimized Hybrid and cumulative Goose graph on full
Apex 32K/512 traces. The snapshots below retain historical
comparisons and implementation context.

## Live Apex dispatch and Fusion audit

The active independent checkpoint is EXP-1603. Compare its recorded
device trace with the frozen EXP-1476 graph-node recording from
EXP-1596. Both use the frozen Apex 32K/512 request, RTX 4070,
4096/4096 batches, Q4_0 K/V and no prompt reuse. These recordings come
from different sessions, use 5,920 versus 4,992 MiB expert budgets,
and may sample different expert routes. The newer Goose trace warns
of incomplete capture. Use the common 448 complete decode steps for
the comparison and the full prefill slices for family attribution.

Decode windows start at each runtime's corresponding GDN marker and
end at the first GDN marker of step 449. The hybrid starts after 90
tail-prefill GDN launches, leaving 15,330 decode markers for 511
steps. The Goose one-token kernel is distinct. GPU-active time is
the union of clipped kernel, copy and memset intervals. Launch
counts are recorded API calls. Compare this window with the older
full request separately.

| Recorded decode activity over 448 steps | Frozen EXP-1476 | EXP-1603 |
|---|---:|---:|
| Direct CUDA launch calls | 6,736 | 929,152 |
| CUDA Graph launch calls | 18,291 | 0 |
| Kernels with nonzero graph node ID | 783,328 | 0 |
| Device window, s | 7.995 | 14.855 |
| Recorded GPU-active time, s | 6.195 | 8.869 |
| Time outside recorded GPU operations, s | 1.801 | 5.986 |
| H2D copies up to 4 KiB | 20,608 | 399,168 |
| H2D total, GB | 45.185 | 34.652 |
| D2H total, GB | 0.446 | 0.446 |

The remaining 4.185 s gap includes host work, submission latency,
stream waits and any missing trace events. The 2.367 s of direct
Goose kernel launch API time overlaps the GPU timeline. The gap
and API time cannot be added or assigned solely to CUDA Graph.
The different cache budgets and continuations also affect copies.

### Graph scopes actually replayed

Join graph-linked kernels to `cudaGraphLaunch` by correlation ID
and order each group by device start. There are 18,291 matching
correlations in the clipped window. The dominant groups contain
44 kernels on 12,936 launches and 43 kernels on 4,014. Both start
with `k_get_rows_float`, run repeated Q8_1 pack and quantized
projection pairs, apply GLU and expert reduction, continue
through dense and recurrent or attention work, and end with
`rms_norm_f32`, `mul_mat_vec_f` and `topk_moe_cuda`.
An additional 446 launches contain 46 kernels ending at Top-K.
The 27- and 20-kernel variants separate decode-step entry and output
work. One group is clipped by the selected window.

The one built standalone capture is the two-node vector Attention
and merge pair in `cuda-driver.c`. It requires `GOOSE_CUDA_GRAPH=1`
and leaves shared query packing outside replay. The 5,110 launches
in EXP-1572 are evidence for that older opt-in package. The
current direct-launch trace has zero. The historical
`goose-graph-lifecycle.goose` manages tensor graph storage and hashes.
It supplies no CUDA Graph capture or executable replay.

### Executed families and reuse candidates

| Scope | Frozen hybrid | Active Goose | Reuse and remaining dependency |
|---|---|---|---|
| Quantized prefill | `mul_mat_q`, 3,392 launches and 3.148 s, plus 480 fixups | MMQ dense and routed cooperative, reference and general families, 13.097 s combined in the recorded prefill | Both Q4_K/Q6_K use shared weight tiles and Tensor Cores. The reference-shaped dense paths preserve K-partition arithmetic, while general Stream-K and fixup use different dispatch. Preserve the tested [32-row tile](CUDA-PARITY-EXP1476.md#dense-q4_k-32-by-128-mmq-tiles) for a scheduling-controlled recheck. |
| Expert grouping in prefill | `mm_ids_helper<8>` runs 936 times in 155.599 ms | Host compact jobs in `graph-dispatch.goose` → `matrix.goose` → `fg_cuda_expert_batch`. Device Q8 packing consumes the uploaded job offsets. No device grouping kernel appears. | The [existing 8-slot device body](../../goose/cuda/goose-cuda-mm-ids/device.goose) produces source, destination and expert ranges. Wire it with SLRU offsets and unchanged job ordering. The hybrid kernel time does not measure removable Goose host time. |
| Decode MMVQ and gate/up Fusion | 175,168 `mul_mat_vec_q` and 175,168 `quantize_q8_1`, 2.341 s combined. The `has_fusion=true` Q4_K specialization accounts for 35,392 matvecs. | 193,088 each of `goose_mmvq_matvec` and `goose_mmvq_quantize`, 2.728 s combined. Separate GLU uses the vector route. | The built [Q4_K device body](../../goose/cuda/goose-cuda-mmvq-q4k/device.goose) is the non-fused specialization. Reuse its dot/reduction, the existing GLU body and the alias rules in [shared Fusion](../../goose/cuda/goose-shared-fusion.goose) after proving the routed and shared-expert pattern. A fused CUDA entry and dual weight lifetime are still required. |
| Decode router fusion | 17,920 `topk_moe_cuda<256,false>` launches, 42.531 ms | 17,920 each of softmax, Top-K and normalized gather, 176.501 ms combined | The [full fused Goose row](../../goose/cuda/goose-cuda-reductions/topk/device-e8.goose) exists. CUDA composition imports only its `topk_winner`. Connect the dual IDs/weights result, handle the output strides and preserve qualified numerics and cache admission. This is a concrete unused implementation. |
| Weighted expert reduction | 17,920 launches, 25.758 ms | 17,920 `goose_moe_weighted`, 42.870 ms | The Goose weighted reduction is already active. Include it when replaying the post-routing graph. |
| RMS, RoPE and KV | 85,568 `rms_norm_f32` (109.709 ms), including multiply and scale specializations. The 8,960 `rope_multi` calls take 9.400 ms. | 85,568 `goose_rms_reference` (239.189 ms), 8,960 `goose_rope_decode` (23.631 ms) and separate cache writes | Weighted RMS and fixed-shape decode IMRoPE already execute. No dedicated RMS–RoPE fused symbol appears in this hybrid window. Use the observed multiply/scale and rotation routes to qualify additional fusion. |
| Flash Attention | Prefill MMA 1.410 s. Decode vector plus merge 1.071 s in 4,480 query/merge pairs. | Whole-tile prefill MMA 2.949 s. Decode prepack, vector and merge execute separately with 4,480 calls each. Vector plus merge is 2.483 s. | Reuse the ready two-node replay as a control and extend only to dependency-safe neighboring work. Stream-K prefill remains experimental after the [isolated recheck](CUDA-PARITY-EXP1476.md#flash-attention-stream-k-on-the-exp-1603-checkpoint). |
| GDN and SSM | 240 `gdn_single` calls, 0.469 s; 240 long SSM calls, 0.205 s | 240 `goose_gdn_mma` calls, 1.349 s; 240 long SSM calls, 0.204 s | Both high-performance families execute. Decode GDN executes 13,440 times on each side in the common window (49.283 to 101.464 ms). Retain the optimized SSM route and capture existing compatible launches. |
| Transfers and waits | Decode graph replay and event ordering are live; 52,418 stream sync API calls | Live deferred matrix waits, async packets and expert upload overlap. Decode still records 22,848 context sync calls and 100,529 synchronous H2D calls. | Start replay after host SLRU slot selection and required upload fences. End at CPU-visible routing, logits or state publication. Keep packet, expert-slot and activation lifetimes valid through graph completion. Transfer and waiting times overlap. |

Prioritize lowering direct submissions for a real decode layer slice.
Use the existing capture lifecycle as a reference. Match graph keys,
addresses, updates and events against the live activation plan before
replaying; exercise shape changes and rejected captures. Measure
direct launches, recorded device gaps and full request performance
separately. The graph counts and kernel times above confirm the gap
in executed infrastructure, with the end-to-end gain of a larger
Goose scope still open.

The source traces are stored as `hybrid-full.nsys-rep` in
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/launch-vram-exp1597`
and `vector-params-apex.nsys-rep` in the sibling
`vector-params-exp1603`. Their SHA-256 values are
`5183929ce3e40ee167eca4533e8b6dfa94d2dfa30d7d8bc7cd62e01ce23f46c9`
and `2e417bfaa61c60b05b082e618f8faacf50e06df2c9d03595a45706b5d9d7eb33`.

## EXP-1596 CPU/GPU profile

This diagnostic compares the immutable EXP-1476 release
(`e7c9155a`) with independent Goose at `4f73f42e`. Both use
the frozen Apex request, RTX 4070, context 262,144, batch and
ubatch 4096/4096, Q4_0 K/V, CPU controls 8/16 and a 4,992 MiB
expert budget. The two runs use gprofng's 2 ms clock collector,
Nsight CUDA kernel-node, API, OS runtime and allocation capture,
and 250 ms process/card memory sampling. The gprofng reports are
filtered between two request signals. Hardware counters and
context-switch sampling remain unavailable on this host. A first
gprofng mode that disabled resource sampling produced no clock
samples and was superseded by the verified configuration.

| Profiled server metric | Native EXP-1476 | Independent Goose |
|---|---:|---:|
| Prefill, tok/s | 2,612.710 | 1,097.321 |
| Decode, steps/s | 56.143 | 26.473 |
| Process RSS high-water, KiB | 22,513,280 | 23,558,448 |
| Sampled peak process / whole-card VRAM, MiB | 8,214 / 10,511 | 7,284 / 9,543 |
| Process swap and request-window system swap-out growth | 0 | 0 |

Phase windows begin at the first full hybrid MMQ or first
Goose RMS and split at the first vector Attention after the
last prefill MMA Attention. Decode ends at the last trace
kernel. The active interval is the union of clipped CUDA
kernel and transfer intervals. Nsight warns of incomplete
Goose capture. Device and host API time can overlap each
other and across threads. These are diagnostic observations.

| Trace metric | Native EXP-1476 | Independent Goose |
|---|---:|---:|
| Prefill span / active GPU / residual, s | 12.164 / 11.054 / 1.110 | 29.716 / 26.110 / 3.607 |
| Decode span / active GPU / residual, s | 9.094 / 7.042 / 2.052 | 18.852 / 10.795 / 8.057 |
| Prefill MMQ computation, s | 3.148 | 13.712 |
| Prefill MMA Attention, s | 1.410 | 2.938 |
| Prefill MMA GDN, s | 0.469 | 1.354 |
| Prefill long SSM, s | 0.205 | 0.204 |
| Decode vector Attention computation, s | 1.214 | 2.715 |
| Decode MMVQ computation, s | 2.488 | 2.767 |
| Decode scalar GDN, s | 0.056 | 0.113 |
| Prefill H2D / D2H, GB | 83.655 / 1.441 | 96.539 / 0.043 |
| Decode H2D / D2H, GB | 51.084 / 0.507 | 52.342 / 0.495 |
| Decode H2D at most 4 KiB, calls | 23,498 | 809,607 |
| Decode CUDA kernel / graph launch API calls | 6,605 / 20,873 | 1,034,583 / 0 |
| Decode sync API time, s | 6.197 stream | 1.151 context |
| Decode H2D API time, s | 0.447 async memcpy including other directions | 9.467 sync + 1.983 async H2D |

Clock samples in the request window total 1.594 s for EXP-1476
and 4.064 s for Goose, including profiler and driver frames.
The hybrid scheduler has 0.374 s of inclusive sampled time
and its graph reserve helper 0.002 s. Goose graph compute has
0.938 s of inclusive sampled time and the quantized gather
worker 0.342 s. The samples support attribution to driver,
submission and execution frames. They cannot be interpreted
as exact aggregate thread CPU or isolated scheduler latency.
The principal measured residuals are MMQ and decode submission
gaps, followed by Attention and prefill GDN. The independent
graph selector is disabled in this fixed profile. The limited
opt-in Goose graph experiment remains documented in the
[CUDA map](CUDA-PARITY-EXP1476.md#cuda-graph-replay-for-vector-attention).

### Device allocation attribution

Reconstruct live device allocations by address and time from
`CUDA_GPU_MEMORY_USAGE_EVENTS`. The 2 MiB VMM intervals in
the hybrid form 20 physically populated 10 MiB spans at
72 MiB logical spacing. One disjoint contiguous 74 MiB span
belongs to its temporary VMM pool. Goose also commits 20
times 10 MiB of physical KV, with no scratch VMM in this run.
Both report 1,440 MiB of logical KV and use 62.8125 MiB of
resident recurrent state. Goose reports those logical sizes
together as 1,575,813,120 bytes.

| Live at tracked peak, MiB | EXP-1476 | Goose |
|---|---:|---:|
| Dense model buffer(s) | 1,302.901 | 1,298.477 |
| Expert device cache | 4,991.484 | 4,991.648 |
| Physical KV VMM pages | 200 | 200 |
| Recurrent state | 62.813 | 62.813 |
| Graph compute buffer or activation arena | 1,347.125 | 384.000 |
| Other ordinary device scratch | 12.126 | 176.842 |
| VMM scratch pool | 74.000 | 0 |
| Total tracked device allocations | 7,990.449 | 7,113.780 |

Compute, activation and scratch storage account for **872.409
MiB** of the 930 MiB difference in sampled process peaks.
The other tracked allocations account for another 4.260 MiB.
The remaining 53.331 MiB is the difference between the two
NVML/trace residuals. This trace cannot separate driver accounting,
additional allocations and sampling alignment in that residual.
It also does not split the hybrid's compute buffer into live
tensors or establish a fragmentation cost.

### Reverse-port audit

The following comparisons cover the active optimization families
in the [CUDA map](CUDA-PARITY-EXP1476.md#mechanisms) and the
[historical ledger](#historical-optimization-ledger). A gap in
coverage or a useful mechanism in Goose is a proposed hybrid
experiment only if the original lacks its effect on the same
workload. The reproduced transfer in EXP-1597 is expert-cache
sizing, with the binary unchanged in a separate copy.

| Mechanism | Evidence in frozen EXP-1476 | Reverse-port disposition |
|---|---|---|
| Tiled Q4_K/Q6_K MMQ, MMA, quantization and Stream-K | The hybrid already runs grouped tiled MMQ with 128-column reuse and reconciliation. Its MMQ device time is 3.148 s versus 13.712 s. | Retain the hybrid kernel. Shift independent Goose dispatch and tile reuse toward the measured winning shapes. |
| RoPE and scalar GDN specialization | The hybrid already uses one RoPE kernel and fixed-width GDN. Goose EXP-1594/1595 restore the selection and register residency. | No demonstrated hybrid gain from these replacements. Hybrid scalar GDN remains faster in the trace. |
| GDN MMA, SSM and graph fusion | Hybrid already selects specialized GDN, fused long SSM SiLU and host-proven fusions. Long SSM ties in the trace. | Test only a measured missing fusion at identical tensor layouts. A wholesale Goose kernel swap would regress the measured GDN path. |
| Shared query packing for long vector Attention | `fattn-vec.cuh` quantizes the query in each partition block. Goose EXP-1582 shares prepacked rows. The hybrid uses fewer partitions and its current combined device path is faster. | Candidate within hybrid CUDA: compare the same Q8_1 bits, remove repeat packing and measure merge/grids and graph replay. No hybrid transfer gain yet measured. |
| Compact expert mapping, shared activation rows, norm/weighted fusion | Hybrid groups on-device with `mm_ids_helper`, has fused top-K and weighted expert reduction. Goose builds compact host jobs with bank offsets. | Do not add a second mapping path. Quantify reuse in the active hybrid specialization before moving host-side grouping. |
| Pinned mmap, async copies, overlap and pinned pageable staging | Both use host pinning and async paths. Hybrid whole trace has 136.880 GB pinned H2D and only 1.387 GB pageable H2D (0.138 s device copy time). | A hybrid ring has a small measured device interval to target. Establish exposed pageable wait before implementation. |
| Small async descriptors, by-value GDN arguments and graph replay | Hybrid already uses kernel parameters, async copies and broad CUDA Graph replay. Standalone still uploads 809,607 small packets in this decode profile. | These are forward-transfer priorities for Goose. Copying the narrower Goose path offers no demonstrated hybrid advantage. |
| Lazy KV VMM and residency budgeting | The two live KV footprints are both 200 MiB. Goose uses a separate context-growth reserve and can use its smaller working arena for more expert slots. | The higher-cache profile is measured separately. KV mapping itself is already live in both. |
| Activation lifetime, scratch and reduced prefill publications | Goose tracks 872.409 MiB less device workspace and records 0.043 GB versus 1.441 GB prefill D2H. | Highest memory-port opportunity. First attribute hybrid peaks to live tensors and track consumers across graph splits. EXP-1559 tested only an arena threshold and changed zero allocations. No hybrid lifetime-allocator port has been measured. |

Keep full CPU/GPU recordings after significant prefill/decode
changes or when the bottleneck moves. Enable `APEX_CPU_PROFILE=1`
and `APEX_SAMPLE_MEMORY=1` for diagnostic captures. For ordinary
performance pairs enable only the memory sampler. The emitted
Goose device image also assembles with `ptxas -arch=sm_86`.
Only the RTX 4070 is executed and timed in these experiments.
Exact commands and the equal-VRAM cache choice are in the
[launch and residency comparison](APEX-LAUNCH-COMPARISON.md).
Use the [current Optimized Hybrid](OPTIMIZED-HYBRID.md) as the
additional performance comparator while retaining the original
EXP-1476 as the frozen control.

## Comparisons and timing boundaries

The reference is the preserved EXP-1476 executable from
[EXP-1559 arm A](../../experiments/EXP-2026-11-20-1559-hybrid-activation-arena/README.md).
The independent checkpoint is
[EXP-1573 arm B](CUDA-PARITY-EXP1476.md#deferred-matrix-waits).
Both traces evaluate the frozen 32,768-input, 512-output
Apex Q4_K_M workload on the RTX 4070, with 4096/4096,
context 262,144 and no cached prompt tokens. They were
recorded in different sessions. [EXP-1576](CUDA-PARITY-EXP1476.md#asynchronous-h2d-packets)
subsequently moves 853,646 H2D packets onto the async API
and saves 2.733 s of decode in an unprofiled pair.
[EXP-1578](CUDA-PARITY-EXP1476.md#cooperative-q6_k-mmq)
reduces Q6_K device MMQ from 5.024 to 4.053 s and
saves 0.966 s of prefill in its unprofiled pair.
[EXP-1579](CUDA-PARITY-EXP1476.md#aligned-dense-mmq-windows)
keeps two-window Q4_K projections on shared MMQ and
saves another measured 0.482 s in its unprofiled pair.
[EXP-1580](CUDA-PARITY-EXP1476.md#q4_k-16-by-64-cooperative-tiles)
reuses the weight fragment for 64-route Q4_K blocks
and saves 2.421 s of full unprofiled prefill.
[EXP-1581](CUDA-PARITY-EXP1476.md#q6_k-16-by-64-cooperative-tiles)
extends 64-route tiles to Q6_K and saves 1.128 s in its
full unprofiled pair.
[EXP-1582](CUDA-PARITY-EXP1476.md#shared-query-packing-for-long-vector-attention)
reuses the query quantization across long vector Attention
partitions and saves 0.655 and 0.728 s of full decode in
opposite-order diagnostic pairs.
[EXP-1583](CUDA-PARITY-EXP1476.md#gdn-launch-parameters)
restores by-value kernel plans from EXP-172. It removes
all explicit GDN descriptor uploads in the short trace
and saves 0.172 and 0.281 s in unprofiled full decode
pairs.
[EXP-1584](CUDA-PARITY-EXP1476.md#pinned-staging-for-pageable-expert-uploads)
restores bounded pageable staging and saves another
3.366 and 3.213 s in full prefill pairs. Peak device
memory stays fixed and the host ring adds about 12 MiB
RSS without swap in either arm. The tables below retain
the original audit checkpoint.
This comparison locates
work and gaps. Final acceptance requires the fresh
paired conditions in [the acceptance checklist](APEX-32K-ACCEPTANCE.md).
The target remains 2,940.9 prefill tok/s and 62.86 decode
steps/s.

| Server metric | Frozen EXP-1476 | Independent Goose | Goose extra time |
|---|---:|---:|---:|
| Prefill | 12.514 s, 2,618.408 tok/s | 41.069 s, 797.870 tok/s | 28.555 s |
| Decode, 511 steps | 9.007 s, 56.733 steps/s | 24.471 s, 20.882 steps/s | 15.464 s |
| RSS high-water | 22,517,948 KiB | 23,514,764 KiB | |
| Sampled peak process VRAM | 8,214 MiB | 8,524 MiB | |
| Process swap and system swap-out growth | 0 | 0 | |

The whole request exhibits an opening prompt echo in Goose.
The experimental three-mechanism package retains this defect.
Its quality and speed gates remain open.

The trace prefill window starts at the first full MMQ in
EXP-1476 and the first reference RMS in Goose. The long
decode window starts at the first multi-part vector FA
after prefill and ends at the final CUDA kernel. Startup
and the hybrid's short warm-up are outside these cuts.
The union measures time with a recorded kernel or DMA
interval on the GPU. The gap is the residual in that trace
window. It includes host-side work and device handoff
latency that need separate attribution. The intervals
can differ from server timing boundaries.

| Trace window | EXP-1476 span / active / gap | Goose span / active / gap |
|---|---:|---:|
| Prefill | 12.119 / 11.015 / 1.104 s | 40.728 / 31.573 / 9.155 s |
| Long decode | 8.998 / 7.039 / 1.960 s | 24.460 / 13.018 / 11.442 s |

## Measured hot paths

Device family times are sums. CUDA API times include
waiting for device work. These metrics overlap with
each other and the trace windows above.

| Area | Measured cost or difference | Cause and restoration check |
|---|---|---|
| Prefill Q4_K/Q6_K MMQ and tiles | MMQ 3.137 s versus 19.371 s, +16.234 s. Packing is 0.458 versus 0.367 s. | EXP-1476 uses shared 128-column tiles with LDSM, MMQ specializations and Stream-K fixup. Current Goose shares weights and scale data for selected Q4_K dense and routed shapes. Q6_K and some Q4_K still use narrow tiles with repeated fragment loads. Bring shared reuse and wider work assignments to the live shapes. Verify kernel dispatch, output, workspace and full prefill. |
| Host submission in long decode | 23,498 versus 844,541 H2D copies up to 4 KiB. Goose spends 11.715 s inside those host calls, with 0.244 s of device copy intervals. The reference's small copy device time is 0.008 s. Long decode gap grows by 9.482 s. | The 208-byte vector plans and 16-/192-byte matrix jobs are uploaded repeatedly on the legacy stream. Event-ordered execution and graph reuse in the hybrid reduce dispatch and packet traffic. Coalesce or retain descriptors, move stable shape decisions to Goose device plans and batch remaining dynamic updates. Use the same packet/shape set and compare GPU gaps, bytes and full decode. |
| Prefill scheduling and transfers | Trace gap +8.051 s. H2D is 83.655 versus 95.174 GB and 3.507 versus 3.949 s of copy intervals. | EXP-1476 splits producer/consumer work and coordinates async copies through backend events. Goose's measured control issues copies and kernels on one legacy stream. Establish a separate ordered upload path for already selected experts and safe turnover points for live device buffers. Measure overlap and exposed wait directly. |
| Decode Attention | Vector FA 1.216 versus 3.510 s, +2.294 s. Merge 0.008 versus 0.051 s. | EXP-1565 reduced local spills and the register count to 80 at runtime. The hybrid trace uses different geometry and 255 registers. Static registers alone do not account for speed. Match actual partition/value-work geometry and measure executed instructions, occupancy and kernel time at the same context. |
| Prefill Attention | MMA 1.398 versus 2.838 s. In the full package diagnostic, Stream-K reduced 2.841 s to 2.395 s including fixup, with unchanged request prefill. | The package restores general and uniform reconciliation. Shape selection, tensor staging and causal pruning remain different. Retain the proven reference tiles, check the missing shapes and verify response quality before promoting a new path. |
| Decode matrix specialization and Fusion | Hybrid MMVQ 2.488 s versus 2.990 s. Goose also runs 40,874 scalar matmuls in 0.800 s, 276,406 vector ops in 0.771 s, and 220,205 MMVQ pack launches in 0.275 s. | The reference matches gate/up, GLU and available shared experts in `ggml_cuda_try_fuse` after alias checks. Standalone uses separate projections on many paths. Determine which graph patterns match on the traced Apex shape and reuse existing Goose MMVQ and fused output arithmetic. Verify full state and output lifetimes. |
| GDN and SSM | GDN prefill 0.468 versus 1.360 s, +0.892 s. Long SSM 0.208 versus 0.204 s. Decode scalar recurrence in Goose consumes 0.610 s, with its precise hybrid counterpart still open. | The long SSM+SiLU path already executes. Compare GDN tile geometry and state write fusion, and qualify the existing scalar and MMA bodies across the context transition. Avoid re-porting the working long SSM kernel. |
| MoE dispatch and routing Fusion | Decode Top-K 0.049 versus 0.081 s. Goose's separate route reduce and normalize add 0.089 and 0.032 s. Hybrid prefill device grouping is 0.155 s. | The hybrid emits groups through `mm_ids_helper`. Goose uses compact host jobs and shared Q8 packing but separate route kernels and host route reads. Connect existing Goose device grouping and fuse compatible routing nodes. Check IDs, weights, empty groups and slot eviction. |
| Expert cache and large H2D | By trace prefill/decode windows Goose moves 11.520 and 7.211 GB more H2D. In the whole Goose trace, SLRU reports 149.376 GB of expert uploads out of 154.864 GB total. | The density/expert placement differs from early CPU-MoE profiles. Measure expert-bank fills, replacement, repeated uploads and lifetime hazards before changing the budget. The Flash Stream-K package changes output and adds 980 fills and 1.639 GB H2D. Treat that as a quality- and workload-dependent comparison. |
| D2H and host logits | Long-decode D2H is 507,226,784 bytes in both traces and 0.041 versus 0.040 s on the device. Goose full D2H is 551.156 MB after removal of large intermediate copies. | Whole logits rows remain needed by the supported sampling path; D2H bandwidth is a lower priority than small H2D dispatch. Investigate GPU scoring only with sampling and response parity. Preserve the existing device-to-device 64 MiB staging gain. |
| Allocators and live tensor reuse | EXP-087/101 reduced historical VRAM. Current Goose graph arena planning costs 0.160 s across decode. VMM scratch proves stable addresses, 11 live maps and no peak increase in the full package. | Reuse the existing lifetime planner and scratch VMM. Restore KV physical-page laziness separately. Reuse plans only when shape and alias ownership permit it. Measure RAM/VRAM, swap, reuse and branch/reset behavior. |
| CUDA Graph | Hybrid trace records 20,914 graph submissions and 29,471 direct launch calls. The Goose two-node Attention capture covers 5,110 submissions. The full package still issues 1,077,590 direct launch calls. | Broaden capture to dependency-safe decode sequences and put changing params in stable device descriptors. Graph creation or parameter updates alone are not a throughput gain. Verify cache growth, pointer/version invalidation and replay. |
| Synchronization | EXP-1573 removes 265,202 context waits. Prefill saves 0.298 s and decode 1.019 s. Host H2D time rises by 23.108 s and copy count/bytes match. | Waiting moves from explicit context synchronization into synchronous H2D. The next candidate needs safe packet residency or async publication, plus explicit visibility at required host reads. |
| Byte packing, guarded code and registers | Current Q4_K shared paths use 110 registers and 5,120 shared bytes. Hybrid MMQ uses 221–255 registers and up to 57,856 shared bytes. The earlier vector FA transformation removes 81 static local instructions and saves 5.543 s of kernel time. | Register counts differ with the work per CTA. Use SASS, spills, shared bank layout and measured work per tile together. A target-wide Nsight Compute comparison with identical shape groups remains unmeasured. |

## CPU and build costs

A full diagnostic wrapped the existing steps with wall and
thread CPU clocks. The diagnostic uses the EXP-1573 archive
and the unchanged request and matches the control response,
IDs, probabilities and stop reason. There are eight prefill
steps and 511 decode steps. The clocks are outside the
individual kernels. Thread CPU time includes driver polling.

| Host scope | Prefill wall | Decode wall |
|---|---:|---:|
| Complete model step | 41.067 s | 22.652 s |
| Graph expansion | 0.002 s | 0.138 s |
| Arena plan and allocation | 0.003 s | 0.160 s |
| Graph compute, including Driver API waits | 41.038 s | 21.845 s |
| Arena and plan free | 0.019 s | 0.0004 s |
| Copy published logits into the output slice | 0.0001 s | 0.190 s |
| Other time in model step | 0.004 s | 0.318 s |
| Server timing outside model step | <0.001 s | 0.766 s |

The decode step consumes 22.647 s of thread CPU time.
System settings deny hardware CPU performance counters.
The measurements still separate graph and arena overhead
from the larger dispatch and waiting region. Equivalent
fine-grained reference CPU timings remain unmeasured.

Full tracked-source build takes 249.27 s and peaks at
1,376,676 KiB host RSS. Reusing the composed unit takes
9.54 s and 222,156 KiB. The composed source is byte
identical across the two unit paths. Generated C matches
after normalizing the embedded path. The fresh binary's
ELF dependencies are libc and libm and its SHA-256 is
`2dbc6d1b101803b0e0e6b139c55894fed3093a4b4c263bfec0084aa315d742fc`.
Profiled CUDA module loading is 8.913 ms in Goose and
13.939 ms across four library loads in EXP-1476. Build
and module initialization costs are outside steady decode.

### CPU batch workers for quantized row gather

EXP-1590 times the CPU arithmetic branch of the regular
graph nodes after input materialization. At 32K/512 the
instrumented run records nine prefill `GET_ROWS` calls
and 67,108,864 FP32 results in 2.655 s of thread CPU
time. Decode gather takes 0.042 s across 511 calls.
The scope excludes CUDA dispatch, synchronization and
other CPU code outside the regular-node arithmetic branch.
The diagnostic reproduces the matched-tail response.

The standalone selector `--threads-batch N` now executes
dense Q4_K/Q6_K row gathers on scoped Goose workers.
The coordinator materializes the inputs in the graph
thread. Each worker uses the existing Goose row decoder
and writes its own output range. The C adapter copies one
packed input row and one output row at a time through the
foreign buffers. CUDA launches and activation state keep
their original owner. The option defaults to one. The
capture runner can set it with `APEX_GOOSE_THREADS_BATCH`.

The CPU fixture checks all 8,382,464 values per weight
type with 1, 3 and 16 threads across 4,093 rows. It
matches bitwise and rejects an out-of-range token ID
with a failed graph result. The HTTP suite verifies
36 requests and rejects invalid worker counts. A short
Apex trace logs one 4,092-row gather on 16 threads.

| Unprofiled Apex 32K/512, 4096/4096, A–B–B–A | 1 thread A | 16 threads B |
|---|---:|---:|
| A–B prefill, tok/s | 1,017.030 | 1,122.879 |
| B–A prefill, tok/s | 1,023.286 | 1,132.302 |
| Median prefill, tok/s | 1,020.158 | 1,127.590 |
| Median prefill, s | 32.121 | 29.061 |
| Median decode, steps/s | 26.907 | 26.995 |
| Process VRAM after request, MiB | 7,272 | 7,272 |
| Process RSS high-water range, KiB | 22,947,728–22,948,244 | 22,952,480–22,954,504 |
| System swap-out, A–B, pages | 0 | 806 |
| Process swap and reverse-pair system swap-out | 0 | 0 |

Prefill saves 3.037 and 3.083 s in the two pairs. All
four responses, token and probability records, expert
cache counters and 5,301,139 resident pages agree.
The clean reverse pair confirms the prefill gain.
The gather work for individual decode tokens stays
serial. The results qualify the batch gather workers.
[EXP-1591](APEX-32K-ACCEPTANCE.md#effective-cpu-profile)
qualifies the 8/16 CPU profile against the effective
inherited affinity of the frozen EXP-1476 workload.

The binary, raw captures and fixtures are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/batch-workers-exp1590`.
Executable SHA-256
`5d3afdef0ad9caab975b010a2f055f343cbb5f9c9908fcf325803a8aa9c177b7`.

## Historical optimization ledger

The audit starts with the published early series, follows
EXP-090 through EXP-172, and checks scoped successors against
the frozen EXP-1476 release (`4e722775`). The archive at that
revision contains 19 files under `patches/` and 92 distinct
September/October IDs under `experiments/`. The direct range
`28a854aa^..5067fe0b` contains 49 experiment-tagged commits;
its later portion already uses Goose inside the hybrid runtime.
The [experiment log](../../experiments/EXPERIMENTS.md) and
[CUDA execution map](CUDA-PARITY-EXP1476.md#mechanisms) retain
the original verdicts and traced backend families respectively.
The ledger groups related changes by mechanism. It counts
five retained and active, fifteen partially restored and five
lost from the standalone path. An alternative implementation
is only marked more effective when same-workload evidence
establishes that comparison. The scoped standalone results
below do not establish any cross-runtime replacement of that kind.

| # | Historical mechanism and reference | Current Goose status and evidence | Restoration, potential and acceptance |
|---:|---|---|---|
| 1 | EXP-087 lazy compute reservation, `c3c52dcc:patches/lazy-pp-reservation.patch`. Historical full prefill peak 6,404 → 4,428 MiB. EXP-121/124 attribute retained memory and propose lifetime scheduling. | **Partially restored.** `graph-storage.goose` and `graph-lifetimes.goose` drive a ~1.010 GB host arena at batch 4096. `cuda-driver.c` uses device lifetimes and adaptive activation storage. | Measure live GPU bytes and release across short → long → short. Preserve the 384 MiB activation checkpoint. The historical 1,976 MiB is a different memory layout. |
| 2 | EXP-089/090 resident mmap pins and MemAvailable admission, `8224fd37:patches/mmap-expert-registration.patch`, `28a854aa:patches/mmap-expert-available-memory.patch`. | **Retained and active on Apex.** `ec_pin_mapped` checks resident pages, registered-byte budget and MemAvailable, then calls CUDA through the adapter. The live bounded mode pins 28 ranges, 4,227,973,120 bytes. | Keep the 4,096 MiB host budget separate from SLRU VRAM. Full pinning regressed memory on the current host. |
| 3 | EXP-101 lazy KV VMM, `87209f12:patches/kv-lazy-vmm.patch`. EXP-111 extends qualification to Flint SWA, with 3,264 → 2,536 MiB in that checkpoint. | **Partially restored.** EXP-1575 keeps Apex Q4 addresses stable and maps only the accessed prefix, saving 1,240 MiB at 32K/512. Reset clears mapped pages, teardown releases them. Flint SWA and near-capacity compatibility lack equivalent independent evidence. | Test near capacity, reset and branch. Keep transpose/layout guards. Qualify SWA separately without changing the Apex path. |
| 4 | EXP-107 dual-slot GPU staging and copy/compute overlap, `d73f7d59:patches/expert-h2d-compute-overlap.patch`. Historical 14.300 → 12.978 s prefill and 1.415 s overlap with CPU MoE. | **Partially restored.** `ec_prepare` and `submit_expert_stage` overlap later pinned planes with MMQ using the existing SLRU bank. EXP-1577 measures 0.440 s overlap on 9.516 GB and 0.511 s lower request prefill. EXP-1584 extends the same overlap to eligible pageable sources. | The current projection and oversized sources remain blocking. Measure exposed waits for those planes before extending scheduling. Total prefill H2D interval in the audit trace is 3.949 s, including traffic already overlapped. |
| 5 | EXP-038/040/041/062 pinned upload ring and depth three, `d965c667`, `9360ba0f`, `9f8748b5:patches/ring-depth-3.patch`. Depth three saved 11.5% median prefill in the historical Qwen profile. | **Partially restored.** EXP-1584 restores the three-slot ring for the live Apex pageable path. It uses 12 MiB of the existing pin budget and saves 3.290 s median full prefill. The historical CPU-MoE Qwen profile has no equivalent live standalone qualification. | Preserve the direct path for already pinned spans. Check the original Qwen layout separately with the same memory and output criteria. |
| 6 | EXP-075 split transfers on pinned-range boundaries, `36edcc67` and the boundary fix `c1d34d27`. EXP-076 deferred registration, `ae8f6510`, `c4a8ff87`. | **Partially restored.** Current Goose admits whole aligned spans and checks every async expert transfer for full containment. It lacks split submissions and registers admitted spans at model preparation. | Only revisit split submission if a trace observes many partial intersections. Profile load and first request before reopening worker registration. The archival async candidate traded a 56.16% slower first request for later gains. |
| 7 | EXP-045 two-worker CPU gather, `73354ac4:patches/two-thread-gather.patch`. The historical offline median gather+upload was 1,924.465 → 1,827.530 ms (three wins in four pairs). The later commit adds a bounded 512 KiB threshold and futex helper to the historical default build. | **Lost on the standalone CUDA expert path.** Planned Goose jobs resolve expert slots on the host. `submit_pageable_expert` uses one `memcpy` into a pinned slot before queueing DMA. [EXP-1590](#cpu-batch-workers-for-quantized-row-gather) parallelizes quantized `GET_ROWS`, a distinct stage. | Instrument exposed gather/copy wall time in the live staging ring first. The 496.453 ms of concurrent GPU idle in EXP-043 is a Qwen observation; the current Apex benefit is unmeasured. |
| 8 | Compact host job mapping and active-expert tiling. The corrected EXP-117 bound is `e200c47d:patches/moe-launch-bound-experimental.patch`. | **Retained and active as a compact schedule in Goose.** `cuda_matrix_planes` emits one job per real route and `fg143_mmq_tiles` ends each tile at its bank-plane boundary. Q4_K and Q6_K tiles execute on real Apex. EXP-117 tested a different grid bound and its corrected result tied at 13.024 vs 13.020 s. | Preserve zero work for empty groups and last partial tiles. Do not recover the earlier stale-grid variant. |
| 9 | GPU grouping and scatter via `mm_ids_helper`. EXP-181 (`a03ddce1`) ported the eight-slot kernel. The later isolated measurement at `37e31805` found no clear gain from the Goose helper's implementation alone. | **Lost from the standalone executor.** The helper executes 936 times in EXP-1476, totaling 154.024 ms. Standalone consumes compact host jobs for the resident tile schedule. The old kernel emits source, destination and bounds arrays; the live MMQ helper uses offsets into the SLRU bank. | Measure current route extraction, list uploads and tile counts on fixed route maps. Connect the existing helper when its maps can replace the host expansion and job transfers. Preserve host SLRU admission and slot fences. The 154.024 ms is the old kernel's cost, not the current removable time. |
| 10 | EXP-160 graph Fusion policy, `59891c83`. Includes safe pattern eligibility and alias checks. | **Partially restored.** Standalone executes weighted RMS, fused SwiGLU and weighted reductions. The full gate/up paired matrix operation and remaining fusion coverage are open. | Reuse Goose numerical bodies and prove graph elision and output lifetime checks. The archived decode trace has 220,205 packing launches at 0.275 s plus vector work at 0.771 s, shared across eligible and ineligible operators. |
| 11 | EXP-170 SSM Fusion, `67ef2ecc`. Rolling serial decode, tiled prefill, six tap sizes, optional bias and SiLU. | **Partially restored.** The required four-tap Apex fused SiLU path is active. The trace sees 240 long convolution launches in 0.204 s, versus 0.208 s in the hybrid. Other tap/layout/bias combinations have no equivalent full standalone qualification. | Preserve the measured shape. Gate other modes separately and prove state at tile and request boundaries. Current Apex time shows no convolution deficit. |
| 12 | EXP-172 GDN scalar, BF16 MMA, fused state and snapshots, `5067fe0b`. Historical 32K/128 with exact output and 127 graph reuses. | **Partially restored.** Standalone runs the same Goose scalar and whole-head V128 MMA bodies. The first-layer values were checked and persistent state publication is live. V32 MMA and multi-snapshot cache semantics lack standalone coverage. | Prefill GDN is 1.360 s versus 0.468 s in the audit trace. Compare actual tile geometry, spills and state publication before changing arithmetic. |
| 13 | EXP-172 GDN launch plan passed by value, `5067fe0b`, `scalar-bridge.cuh` and `mma-bridge.cuh`. | **Retained and active after restoration.** EXP-1582 issued a separate 456-byte H2D packet per GDN launch. EXP-1583 removes 3,870 such copies in the short live trace and saves 0.172/0.281 s full decode with the same response and memory envelope. | Preserve the direct CUDA ABI around existing Goose math. At 240 prefill and 15,330 decode recurrence launches the eliminated explicit packet payload is 7,099,920 bytes. |
| 14 | EXP-161 allocator, EXP-163 ordering, EXP-164 graph scheduler. Later EXP-195 mapped pool arithmetic, `3ac0df66`. | **Partially restored.** Independent Goose reuses graph storage and lifetime planning. Scratch VMM is available experimentally and lazy KV uses stable reservations by default. Allocation and planning cost 0.160 s in the archived decode trace. | Reuse plan state where tensor shapes, views and aliases match. Measure physical maps, idle retained VRAM and full request impact. EXP-093 rejected generic pool trimming on its small measured bound. |
| 15 | CUDA Graph replay on hybrid EXP-172 and EXP-1476. EXP-049–052 diagnose verify-shape alternation; EXP-052 rejects its L2-only two-variant candidate. The separate EXP-059 `GGML_CUDA_GRAPH_OPT` screen also regresses. | **Partially restored.** Standalone can capture and update a two-node vector Attention pair. Full package trace observes 5,110 graph launches, compared with 20,914 graph submissions across scopes in EXP-1476. | Reuse tested graph lifecycle and extend capture with explicit pointer/geometry updates and expert-slot fences. The audit measures 11.442 s of idle trace gap in long Goose decode, including other waits and handoffs. The whole trace's 2.933 s of direct launch API time is another overlapping bound. |
| 16 | EXP-083 RAM-only recurrent/SWA prefix checkpoints, `8c1f02e9`. EXP-1464 `4e7fad74` reused 11,697 of 11,705 tokens on a follow-up. | **Lost from independent serving.** `q35_sample_generate` resets state at each request. The HTTP handler accepts `cache_prompt=false` and does not expose a live warm prefix path. | Reuse prefix policy and KV range logic. Restore recurrent state together with KV and guard cache ownership, divergence and cancellation. The historical 11,697-token saving applies to warm prefill only. |
| 17 | EXP-1422/1423 SLRU replacement/residency. In one hybrid A/B 4,992 MiB gave 83.68% hits and 61.83 decode versus 71.25% and 47.43 at 3,072 MiB. | **Retained and active on Apex.** `ec_prepare` calls the preserved Goose SLRU and the live bank is 4,991.65 MiB. Declared SLRU capacity and registered host spans are independent. | Preserve policy and test any new cache budget against real uploads, process VRAM and the matched model profile. Current ranges cannot explain a speed change by capacity alone. |
| 18 | EXP-046 fused IQ2_S verify decode, `a11a0e52:patches/iq2s-fused-decode-default-on.patch`, preserved by EXP-079 on the old Qwen path. | **Lost on the old Qwen fast path.** Standalone checked MMVQ layout selects Q4_K and Q6_K. Codec IQ2_S presence provides no evidence of the fused verify computation or its speed. | Evaluate the originally supported Qwen profile and reuse the Goose IQ2_S arithmetic only if a real verify workload exercises it. Apex Q4_K_M offers no estimate. |
| 19 | EXP-006 split model placement and mmap ownership, `4e722775:patches/expert-tier-integration.patch`. The original Qwen test retained speed with 12.9 GiB less RAM. | **Partially restored.** Apex loads all pages in a file-backed mapping, retains supported dense matrices on the GPU and admits selected expert planes into SLRU. Separate standalone evidence for Qwen SSD streaming, row windows and tier eviction is missing. | Keep one runtime and verify model-specific placement independently. The Qwen RAM result is scoped to its original model and load mode. |
| 20 | EXP-009 GPU reserve and bounded autofit (`75fed99d`) and EXP-121 physical-device attribution. The historical launcher passes an explicit free-VRAM reserve; EXP-010 leaves EHS off. | **Partially restored.** Goose budgets dense placement and SLRU capacity using the live CUDA free-byte query plus expected state/head space. The lazy path previously subtracted logical KV bytes from the reserve even though those bytes were still free. See [the VRAM admission correction](#lazy-kv-vram-admission). | Keep lazy KV, account separately for already committed pages and subsequent growth, and verify the configured 4,992 MiB bank still fits on the Apex profile. Historical EHS admission offers no Apex throughput estimate. |
| 21 | EXP-024 split MTP draft (`92406443`, `patches/mtp-sidecar.patch`), accepted configuration with 13.6% warmer Qwen decode on its own profile. | **Lost from standalone inference.** The Apex trunk loader recognizes and skips MTP tensors. The fixed EXP-1476 comparison profile uses `--spec-type none`. | Restore only alongside the model-specific verify path and comparable quality. Expected effect on the specified Apex acceptance profile is zero because it does not use a draft. |
| 22 | EXP-013 CPU thread scaling (`39cc4735`), EXP-057 affinity and pool placement. EXP-092 rejects a 12-worker Apex promotion after a fresh control. | **Partially restored.** EXP-1590 and EXP-1591 expose and exercise the real 8/16 profile. Some independent single-token graph paths remain serial, and effective affinity for the compared hybrid was inherited. | Extend worker use only where CPU profiling identifies real work. The 16-worker quantized gather saves 3.037/3.083 s on full Apex prefill; those gains are already present in the comparison executable. |
| 23 | EXP-171 IMRoPE (`5c885dc3`) and the active CUDA `rope_multi` path. | **Retained and active for Apex.** `q35_attn_project` calls the positional rotation. The standalone runtime runs its Goose CUDA RoPE arithmetic and reference FWHT rotations on the live Q4 KV path. | Preserve rotary-section layout and position offsets. Remaining norm/cache fusion and alternate layouts are tracked in parity items 19 and 22. |
| 24 | EXP-173 Flash Attention prefill and decode, with the frozen EXP-1476 causal `4 × 8` and `8 × 8` families. | **Partially restored.** Existing CUDA vector decode and large-prompt MMA paths are live. [EXP-1592](CUDA-PARITY-EXP1476.md#four-query-flash-attention-checkpoint-tail) restores and traces the four-query tile on real Apex under a diagnostic selector. | The scoped oracle's max error is `5.96e-8`; the full candidate improves the record count but loses 1.056 s in decode with a different token sequence and expert workload. Keep the candidate reproducible without adopting it as the default. |
| 25 | EXP-138–149 host CPU compute ownership (`ccde6f61` through `2a0c4b77`), including AVX2 tiled quantized CPU MoE, quant type dispatch and threadpool. | **Partially restored.** The standalone CPU path runs Goose quantized decode, matrix routing and row workers. Historical hybrid AVX2 and worker policies must be checked against actually dispatched standalone shapes. | Compare instruction mix, layout and worker duty on active CPU operators. Current Apex quantized matrix time is dominated by GPU kernels; no isolated CPU MoE gain is measured for this profile. |

### Historical negative results and estimates

The October archive includes rejected or unfinished work that
does not represent a removed production optimization. EXP-086
and EXP-112 tested 6144 batches with memory or decode
drawbacks; EXP-125 measured a 4096 → 2048 prefill drop from
1,719 to 1,246 tok/s. EXP-088, EXP-091–093, EXP-096–100,
EXP-102/103/106/108/117/132 and EXP-078 retain negative
or inconclusive evidence in the
[experiment log](../../experiments/EXPERIMENTS.md) and their
individual cards. EXP-109/110/113–116, EXP-122–124,
EXP-126–131 and EXP-133 are proposals, resource audits or
bounds. In particular EXP-122's idle KV unmap was never an
accepted runtime feature; EXP-126's prefill CUDA Graph idea
does not prove an original prefill replay benefit. Earlier
rejected Qwen speculation, placement, affinity and ring-depth
variants likewise do not represent deletion regressions.

### Integration order

Track historic mechanisms through the shared
[CUDA parity map](CUDA-PARITY-EXP1476.md#mechanisms).

The EXP-1592 diagnostic supplies a newer whole-trace view of
the dominant families. The table sums device intervals by
short kernel name. The runs are from separate sessions and
the completion token streams differ. Small startup work can
contribute to both sums. The deltas are priorities for isolation
and do not add up to a change in request latency.

| Device family, seconds | EXP-1476 | EXP-1592 | Difference |
|---|---:|---:|---:|
| MMQ matmul, separately from quantization/packing | 3.137 | 13.704 | +10.566 |
| MMA Flash Attention, including partition fixups | 1.400 | 2.946 | +1.546 |
| Vector Flash Attention body | 1.216 | 2.774 | +1.558 |
| Whole-head prefill GDN MMA | 0.468 | 1.354 | +0.887 |
| Scalar GDN | 0.057 | 0.475 | +0.418 |
| RoPE, phase generation plus application in Goose | 0.022 | 0.539 | +0.517 |
| Long SSM convolution | 0.208 | 0.204 | −0.004 |

For RoPE the independent trace has 10,326 calls per phase,
compared with 10,440 calls to the hybrid's `rope_multi`.
[EXP-1594](CUDA-PARITY-EXP1476.md#single-kernel-imrope-for-apex-decode)
restores a single kernel for the active decode layout,
reuses the existing phase recurrence, and saves 0.471 s
median full Apex decode. The table retains its original
diagnostic checkpoint. Multi-token RoPE and other
norm/cache fusion remain separate.

1. Reprofile the current 8/16 checkpoint with fixed routes or
   teacher-forced continuation for attribution. Reuse shared
   Q4_K/Q6_K MMQ and measure remaining kernel time and exposed
   staging waits. The old 16.234 s MMQ difference includes
   gains already restored by EXP-1578–1581 and EXP-1587.
2. Extend CUDA Graph capture to a stable decode sequence where
   the replay replaces actual submissions. Gate pointer, shape,
   cache growth and expert-slot eviction before larger captures.
   Compare submission gaps and full decode on the same request.
3. Compare equivalent GDN decode and prefill shapes using the
   existing EXP-172 bodies. Measure registers, launches and
   reduction/state traffic before changing arithmetic.
4. Measure pinned-slot preparation and exposed DMA waits on
   pageable expert fills. Consider the original EXP-045 worker
   only if this producer is on the critical path. Keep fixed
   bytes, expert IDs and slot reuse in that comparison.
5. Evaluate the existing device grouping kernel only when its
   outputs eliminate host job expansion or transfer. Compact
   host jobs and live top-K already run. Validate empty groups,
   repeated routes and destination ordering, with SLRU slot
   ownership preserved.
6. Recover warm KV/recurrent prefix reuse separately from the
   cold benchmark. Check exact-prefix hits, divergence and
   request cancellation. Qualify IQ2_S verify, MTP and streamed
   expert placement on the original supported Qwen profile.
   Cold Apex with `cache_prompt=false` and no draft cannot
   measure these gains.

Cold Apex acceptance remains 32K/512, 4096/4096, fresh A/B
against EXP-1476, with output quality, RSS, VRAM and swap under
identical conditions. The EXP-1591 full reference replay
measures 2,674.379 prefill tok/s and 61.647 decode steps/s.
The newer [EXP-1592 scalar control](CUDA-PARITY-EXP1476.md#four-query-flash-attention-checkpoint-tail)
measures 1,119.964 and 26.993. The separate observations
differ by 17.006 s in prefill and 10.642 s in decode. They
are neither a fresh pair nor an allocation of loss to any
single mechanism.

### Lazy KV VRAM admission

The Goose dense and SLRU placement functions both call
`mr_cuda_reserve` after preparing CUDA model state. The old
subtraction used the logical state size for both eager and lazy
allocation. Under lazy VMM, `cuMemGetInfo` still counts the bytes
that the model will map later as free. A sufficiently large
resident expert bank could spend those bytes at startup and
leave the configured context without VRAM to grow.

EXP-1593 uses the already committed startup allocation
in the placement calculation. The startup allocation is the
entire state for eager mode and the recurrent state for lazy
mode. Mapping remains on demand. The logical size reported via
the existing runtime statistics keeps its original meaning.
The CUDA fixture checks 10,486,272 logical bytes in both modes,
with 10,486,272 eager startup bytes or 512 lazy startup bytes.
Data survives growth and reset clears KV and recurrent state.
Compute Sanitizer reports zero errors.

A startup-only Apex boundary check uses the same profile at
7,168 MiB of explicitly requested expert cache. The old build
becomes ready. The corrected build exits with a capacity message
reporting 6,635,413,504 bytes available after model allocations.
It also rejects a 7,992 MiB request before allocating the bank.
The boundary result establishes admission behavior. No inference
request was sent at either raised budget.

| Full cold Apex 32K/512, 4096/4096, 4,992 MiB expert budget | Control A | Corrected B |
|---|---:|---:|
| Prefill, tok/s | 1,128.516 | 1,121.384 |
| Decode, steps/s | 26.985 | 26.975 |
| Process RSS high-water, KiB | 22,956,272 | 22,955,328 |
| Process VRAM ready / after request, MiB | 6,510 / 7,272 | 6,510 / 7,272 |
| Device expert bank, bytes | 5,234,122,752 | 5,234,122,752 |
| Logical KV/SSM state, bytes | 1,575,813,120 | 1,575,813,120 |
| Resident model pages | 5,301,139 | 5,301,139 |
| Process swap and system swap-out growth | 0 | 0 |

Both answers, all 512 IDs, probability rows, stop reason and
SLRU counters are identical. The existing content defect is
unchanged. This single pair verifies the supported configuration
and reports the observed 0.185 s more prefill and 0.007 s more
decode. It does not establish a throughput trend. VRAM figures
are snapshots, with continuous peak sampling still open for
final acceptance. The process maps have no libggml or libstdc++.

The frozen request SHA-256 is
`484a12de00ec057e4b78d480ebb185153bb10af607eb23192a72995ba3026d0f`.
The corrected executable SHA-256 is
`407b5b304df32e6966d388f6cee93acf582cbf4d627ee020892279966caa8c1d`.
Raw runs, both binaries, the fixture log and output checksums
are under
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/vram-reserve-exp1593`.

Reference artifacts are in
`/home/end0/AI/LlamaServer/franken-exp1476-arena-ab.htNK9R/a`.
Goose traces and offline interval analysis are in
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/matrix-waits-exp1573`.
CPU wrapper and build evidence are in
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/performance-audit-exp1574`.
