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

## Historical optimization ledger

The ledger follows the Apex optimization line from EXP-090
and its dependencies through EXP-172. It also checks
successors retained in the EXP-1476 reference. The earlier
Qwen experiments identify shared mechanisms and
model-specific work. The separate [CUDA execution map](CUDA-PARITY-EXP1476.md#mechanisms)
covers the remaining backend families from the frozen
hybrid trace. The evidence column distinguishes archival
results, current execution, and conditions that remain
unmeasured. Baseline numbers from different model placement,
profiles or sessions do not establish a paired speed delta.
The 18 entries comprise four retained and active, ten
partially restored and four lost from the live path.

| # | Historical mechanism and reference | Current Goose status and evidence | Restoration, potential and acceptance |
|---:|---|---|---|
| 1 | EXP-087 lazy compute reservation, `c3c52dcc:patches/lazy-pp-reservation.patch`. Historical full prefill peak 6,404 → 4,428 MiB. EXP-121/124 attribute retained memory and propose lifetime scheduling. | **Partially restored.** `graph-storage.goose` and `graph-lifetimes.goose` drive a ~1.010 GB host arena at batch 4096. `cuda-driver.c` uses device lifetimes and adaptive activation storage. | Measure live GPU bytes and release across short → long → short. Preserve the 384 MiB activation checkpoint. The historical 1,976 MiB is a different memory layout. |
| 2 | EXP-089/090 resident mmap pins and MemAvailable admission, `8224fd37:patches/mmap-expert-registration.patch`, `28a854aa:patches/mmap-expert-available-memory.patch`. | **Retained and active on Apex.** `ec_pin_mapped` checks resident pages, registered-byte budget and MemAvailable, then calls CUDA through the adapter. The live bounded mode pins 28 ranges, 4,227,973,120 bytes. | Keep the 4,096 MiB host budget separate from SLRU VRAM. Full pinning regressed memory on the current host. |
| 3 | EXP-101 lazy KV VMM, `87209f12:patches/kv-lazy-vmm.patch`. EXP-111 extends qualification to Flint SWA, with 3,264 → 2,536 MiB in that checkpoint. | **Partially restored.** EXP-1575 keeps Apex Q4 addresses stable and maps only the accessed prefix, saving 1,240 MiB at 32K/512. Reset clears mapped pages, teardown releases them. Flint SWA and near-capacity compatibility lack equivalent independent evidence. | Test near capacity, reset and branch. Keep transpose/layout guards. Qualify SWA separately without changing the Apex path. |
| 4 | EXP-107 dual-slot GPU staging and copy/compute overlap, `d73f7d59:patches/expert-h2d-compute-overlap.patch`. Historical 14.300 → 12.978 s prefill and 1.415 s overlap with CPU MoE. | **Partially restored.** `ec_prepare` and `submit_expert_stage` overlap later pinned planes with MMQ using the existing SLRU bank. EXP-1577 measures 0.440 s overlap on 9.516 GB and 0.511 s lower request prefill. EXP-1584 extends the same overlap to eligible pageable sources. | The current projection and oversized sources remain blocking. Measure exposed waits for those planes before extending scheduling. Total prefill H2D interval in the audit trace is 3.949 s, including traffic already overlapped. |
| 5 | EXP-038/040/041/062 pinned upload ring and depth three, `d965c667`, `9360ba0f`, `9f8748b5:patches/ring-depth-3.patch`. Depth three saved 11.5% median prefill in the historical Qwen profile. | **Partially restored.** EXP-1584 restores the three-slot ring for the live Apex pageable path. It uses 12 MiB of the existing pin budget and saves 3.290 s median full prefill. The historical CPU-MoE Qwen profile has no equivalent live standalone qualification. | Preserve the direct path for already pinned spans. Check the original Qwen layout separately with the same memory and output criteria. |
| 6 | EXP-075 split transfers on pinned-range boundaries, `36edcc67` and the boundary fix `c1d34d27`. EXP-076 deferred registration, `ae8f6510`, `c4a8ff87`. | **Partially restored.** Current Goose admits whole aligned spans and checks every async expert transfer for full containment. It lacks split submissions and registers admitted spans at model preparation. | Only revisit split submission if a trace observes many partial intersections. Profile load and first request before reopening worker registration. The archival async candidate traded a 56.16% slower first request for later gains. |
| 7 | EXP-045 two-worker CPU gather, `73354ac4:patches/two-thread-gather.patch`. | **Lost on the standalone CUDA expert path.** Planned Goose jobs resolve expert slots on the host. There is no two-worker pageable staging copy equivalent in `ec_prepare`. | Establish host gather/copy wall time on pageable fills first. The historical 496.453 ms of concurrent GPU idle from EXP-043 is a Qwen window, with no speed estimate licensed for Apex. |
| 8 | Compact host job mapping and active-expert tiling. The corrected EXP-117 bound is `e200c47d:patches/moe-launch-bound-experimental.patch`. | **Retained and active as a compact schedule in Goose.** `cuda_matrix_planes` emits one job per real route and `fg143_mmq_tiles` ends each tile at its bank-plane boundary. Q4_K and Q6_K tiles execute on real Apex. EXP-117 tested a different grid bound and its corrected result tied at 13.024 vs 13.020 s. | Preserve zero work for empty groups and last partial tiles. Do not recover the earlier stale-grid variant. |
| 9 | GPU grouping and scatter via `mm_ids_helper`. EXP-181 already ported the helper to `goose/cuda/goose-cuda-mm-ids`. The isolated measurement at `37e31805` found no clear gain. | **Lost from the standalone executor.** The helper executes 936 times in EXP-1476, totaling 154.024 ms. Standalone consumes compact host jobs for the resident tile schedule. | Connect the existing helper only where its outputs replace work. Host SLRU still needs active IDs, so adding a second grouping pass establishes no benefit. Measure route extraction, list upload and tile count on identical route maps first. |
| 10 | EXP-160 graph Fusion policy, `59891c83`. Includes safe pattern eligibility and alias checks. | **Partially restored.** Standalone executes weighted RMS, fused SwiGLU and weighted reductions. The full gate/up paired matrix operation and remaining fusion coverage are open. | Reuse Goose numerical bodies and prove graph elision and output lifetime checks. The archived decode trace has 220,205 packing launches at 0.275 s plus vector work at 0.771 s, shared across eligible and ineligible operators. |
| 11 | EXP-170 SSM Fusion, `67ef2ecc`. Rolling serial decode, tiled prefill, six tap sizes, optional bias and SiLU. | **Partially restored.** The required four-tap Apex fused SiLU path is active. The trace sees 240 long convolution launches in 0.204 s, versus 0.208 s in the hybrid. Other tap/layout/bias combinations have no equivalent full standalone qualification. | Preserve the measured shape. Gate other modes separately and prove state at tile and request boundaries. Current Apex time shows no convolution deficit. |
| 12 | EXP-172 GDN scalar, BF16 MMA, fused state and snapshots, `5067fe0b`. Historical 32K/128 with exact output and 127 graph reuses. | **Partially restored.** Standalone runs the same Goose scalar and whole-head V128 MMA bodies. The first-layer values were checked and persistent state publication is live. V32 MMA and multi-snapshot cache semantics lack standalone coverage. | Prefill GDN is 1.360 s versus 0.468 s in the audit trace. Compare actual tile geometry, spills and state publication before changing arithmetic. |
| 13 | EXP-172 GDN launch plan passed by value, `5067fe0b`, `scalar-bridge.cuh` and `mma-bridge.cuh`. | **Retained and active after restoration.** EXP-1582 issued a separate 456-byte H2D packet per GDN launch. EXP-1583 removes 3,870 such copies in the short live trace and saves 0.172/0.281 s full decode with the same response and memory envelope. | Preserve the direct CUDA ABI around existing Goose math. At 240 prefill and 15,330 decode recurrence launches the eliminated explicit packet payload is 7,099,920 bytes. |
| 14 | EXP-161 allocator, EXP-163 ordering, EXP-164 graph scheduler. Later EXP-195 mapped pool arithmetic, `3ac0df66`. | **Partially restored.** Independent Goose reuses graph storage and lifetime planning. Scratch VMM is available experimentally and lazy KV uses stable reservations by default. Allocation and planning cost 0.160 s in the archived decode trace. | Reuse plan state where tensor shapes, views and aliases match. Measure physical maps, idle retained VRAM and full request impact. EXP-093 rejected generic pool trimming on its small measured bound. |
| 15 | CUDA Graph replay on hybrid EXP-172 and EXP-1476. Separately, EXP-052 rejected an L2-only alternating-shape cache. | **Partially restored.** Standalone can capture and update a two-node vector Attention pair. Full package trace observes 5,110 graph launches, compared with 20,914 graph submissions across scopes in EXP-1476. | Reuse tested graph lifecycle and extend capture with explicit pointer/geometry updates and expert-slot fences. The audit measures 11.442 s of idle trace gap in long Goose decode, including waits and work beyond graph submission. |
| 16 | EXP-083 RAM-only recurrent/SWA prefix checkpoints, `8c1f02e9`. EXP-1464 `4e7fad74` reused 11,697 of 11,705 tokens on a follow-up. | **Lost from independent serving.** `q35_sample_generate` resets state at each request. The HTTP handler accepts `cache_prompt=false` and does not expose a live warm prefix path. | Reuse prefix policy and KV range logic. Restore recurrent state together with KV and guard cache ownership, divergence and cancellation. The historical 11,697-token saving applies to warm prefill only. |
| 17 | EXP-1422/1423 SLRU replacement/residency. In one hybrid A/B 4,992 MiB gave 83.68% hits and 61.83 decode versus 71.25% and 47.43 at 3,072 MiB. | **Retained and active on Apex.** `ec_prepare` calls the preserved Goose SLRU and the live bank is 4,991.65 MiB. Declared SLRU capacity and registered host spans are independent. | Preserve policy and test any new cache budget against real uploads, process VRAM and the matched model profile. Current ranges cannot explain a speed change by capacity alone. |
| 18 | EXP-046 fused IQ2_S verify decode, `a11a0e52:patches/iq2s-fused-decode-default-on.patch`, preserved by EXP-079 on the old Qwen path. | **Lost on the old Qwen fast path.** Standalone checked MMVQ layout selects Q4_K and Q6_K. Codec IQ2_S presence provides no evidence of the fused verify computation or its speed. | Evaluate the originally supported Qwen profile and reuse the Goose IQ2_S arithmetic only if a real verify workload exercises it. Apex Q4_K_M offers no estimate. |

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
Prioritize remaining long-decode submission and prefill
MMQ traffic by measured request-window time. After the
GDN descriptor change, target expert copies that still block compute and
compare device grouping only after accounting for host SLRU
inputs. Broader CUDA Graph capture depends on stable buffers,
safe dynamic parameter updates and event ownership.
Warm prefix reuse and non-Apex compatibility need separate
request profiles. Cold Apex acceptance remains 32K/512,
4096/4096, fresh A/B against EXP-1476, with output quality,
RSS, VRAM and swap under identical conditions.

Reference artifacts are in
`/home/end0/AI/LlamaServer/franken-exp1476-arena-ab.htNK9R/a`.
Goose traces and offline interval analysis are in
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/matrix-waits-exp1573`.
CPU wrapper and build evidence are in
`/home/end0/AI/LlamaServer/franken-goose-mmq-coop.gZoAAj/performance-audit-exp1574`.
