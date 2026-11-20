# Apex runtime performance and historical optimization audit

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
full unprofiled pair. The
tables below retain the original audit checkpoint.
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

## Historical optimization ledger

The rows below cover the named early checkpoints and the
mechanisms they depend on. Historical metrics keep their
original profiles. Current Apex runs use a resident GPU
expert bank. The older CPU-MoE experiments used different
weight placement and output length.

| Historical work | Current status and evidence | Restoration or qualification |
|---|---|---|
| [EXP-087 lazy compute reservation](../../experiments/EXP-2026-10-30-087-compute-reservation-feasibility.md) | Partially restored. Historical peak 6,404 to 4,428 MiB with the old CPU-MoE profile. Current Goose grows its resident arena from 64 to 384 MiB and has optional scratch VMM. The full D2H savings from arena growth are already measured. | Reuse computed lifetimes and physical reservation checks. Qualify 262,144 logical context without imposing a fixed reduction to the context or batch. |
| [EXP-089 mmap registration](../../experiments/EXP-2026-10-30-089-cpu-weight-host-residency.md) and [EXP-090 available-RAM admission](../../experiments/EXP-2026-10-30-090-mmap-available-memory.md) | Retained and active in bounded form. Patch history preserves the removal of the raw-free-memory veto and the resident-page and MemAvailable checks. Current Goose pins 28 spans covering 4,227,973,120 bytes, with zero process swap. Full registration was rejected on the current system. | Keep 4,096 MiB host pin. Existing pinning and SLRU are separate budgets. |
| [EXP-101 lazy physical KV VMM](../../experiments/EXP-2026-10-30-101-kv-lazy-vmm.md) | Restored and active on the measured Apex Q4 path. Historical peak falls 4,428 to 3,188 MiB. [EXP-1575](CUDA-PARITY-EXP1476.md#lazy-q4-kv-vmm-restoration) restores the same 1,240 MiB saving with resident experts and exact 32K/512 plus reset output. | Qualify near-capacity generation and remaining model layouts. Measure memory reuse before spending the released capacity on a larger cache or staging. |
| [EXP-107 H2D/compute overlap](../../experiments/EXP-2026-10-30-107-h2d-compute-overlap.md) | Restored for eligible pinned prefill fills on the existing SLRU bank. Historical prefill median falls 14.300 to 12.978 s with 1.415 s of copy/kernel intersection. [EXP-1577](CUDA-PARITY-EXP1476.md#overlapped-expert-projection-uploads) observes 0.440 s of intersection and saves 0.511 s of prefill in a full unprofiled pair with exact output and unchanged VRAM. | Keep slot ownership and event ordering. Reduce the blocking cost of the current projection and pageable spans. Measure each change with the same SLRU budget. |
| [EXP-117 launch bound](../../experiments/EXP-2026-10-31-117-moe-dispatch-prefill.md) and later `mm_ids_helper` | Partially restored as a different execution path. EXP-117's corrected grid-bound prototype tied at 13.020 versus 13.024 s. The earlier faster variant used stale captured geometry. Current Goose emits compact host jobs and routes by expert plane. Frozen EXP-1476 also performs device grouping. | Preserve current compact jobs. Reuse Goose device grouping in the active executor, verify empty and sparse routes and invalidate or update graph geometry when the bucket changes. |
| [EXP-160 shared Fusion](../../experiments/EXP-2026-11-08-160-goose-shared-fusion/README.md) | Partially restored. The hybrid's shared/routed gate/up matcher checks alias ranges. Independent Goose runs first-layer checked projections, SwiGLU and fused weighted reduction. Full two-output fusion coverage is open. | Connect eligible existing fusion arithmetic with graph-level alias guards and trace witnesses, including skipped nodes. |
| [EXP-170 SSM Fusion](../../experiments/EXP-2026-11-08-170-goose-cuda-ssm-convolution/README.md) | Retained for the measured four-tap Apex path. The earlier hybrid run observes tiled prefill and serial decode with SiLU. Current Goose observes 240 long convolution launches at 0.204 s versus reference 0.208 s. | Preserve the working path. Qualify remaining shapes and the state transition. |
| [EXP-172 GDN scalar and MMA](../../experiments/EXP-2026-11-08-172-goose-cuda-gdn/README.md) | Partially restored. Older hybrid scoped gate observes exact 32K/128 outputs and 127 graph reuses. Both Goose kernels now execute; the first recurrent layer passes numerical comparisons. Prefill GDN still costs +0.892 s in the current trace. | Preserve existing scalar/MMA bodies. Qualify fused state snapshots, the supported shape dispatch and long generation. |
| EXP-1476 cached execution and SLRU | Partially restored. Current Goose SLRU is live at 4,991.65 MiB, and vector Attention graph capture has 5,110 launches on the full package. The hybrid's larger replay scopes remain open. | Extend capture and update protocol without assuming immutable jobs, KV prefixes or expert slot ownership. Qualify the combined checkpoint. |

The useful restoration candidates above extend the
[CUDA parity map](CUDA-PARITY-EXP1476.md). The measured order
for speed is MMQ tile throughput and host submission, then
long Attention/GDN and fusion. Lazy KV opens a memory lever
for residency and overlaps. The historical ledger and
all-model coverage remain open beyond this initial set.

Reference artifacts are in
`/home/end0/AI/LlamaServer/franken-exp1476-arena-ab.htNK9R/a`.
Goose traces and offline interval analysis are in
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/matrix-waits-exp1573`.
CPU wrapper and build evidence are in
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/performance-audit-exp1574`.
